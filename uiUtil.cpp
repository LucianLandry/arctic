//--------------------------------------------------------------------------
//                uiUtil.cpp - UI-oriented utility functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <ctype.h>  // tolower()
#include <errno.h>  // errno
#include <stdarg.h> // va_list
#include <stdio.h>  // sprintf()
#include <stdlib.h> // abs()
#include <thread>

#include "clockUtil.h"
#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "stringUtil.h"
#include "ui.h"
#include "uiUtil.h"

using arctic::ToCoord;
using arctic::Semaphore;

UIFuncTableT *gUI;

struct PieceAsciiMap
{
    Piece piece;
    char ascii;
};

static PieceAsciiMap gPieceUITable[] =
{
    {Piece(0, PieceType::Empty),  ' '},
    {Piece(0, PieceType::King),   'K'},
    {Piece(1, PieceType::King),   'k'},
    {Piece(0, PieceType::Pawn),   'P'},
    {Piece(1, PieceType::Pawn),   'p'},
    {Piece(0, PieceType::Knight), 'N'},
    {Piece(1, PieceType::Knight), 'n'},
    {Piece(0, PieceType::Bishop), 'B'},
    {Piece(1, PieceType::Bishop), 'b'},
    {Piece(0, PieceType::Rook),   'R'},
    {Piece(1, PieceType::Rook),   'r'},
    {Piece(0, PieceType::Queen),  'Q'},
    {Piece(1, PieceType::Queen),  'q'}
};
static const int gPieceUITableNumElements =
    sizeof(gPieceUITable) / sizeof(PieceAsciiMap);

char nativeToAscii(Piece piece)
{
    for (int i = 0; i < gPieceUITableNumElements; i++)
    {
        if (piece == gPieceUITable[i].piece)
        {
            return gPieceUITable[i].ascii;
        }
    }
    assert(0); // all pieces should be represented in the table.
    return ' '; // empty square
}

char nativeToBoardAscii(Piece piece)
{
    char ascii = nativeToAscii(piece);
    return piece.IsPawn() ? tolower(ascii) : toupper(ascii);
}

Piece asciiToNative(char ascii)
{
    for (int i = 0; i < gPieceUITableNumElements; i++)
    {
        if (ascii == gPieceUITable[i].ascii)
        {
            return gPieceUITable[i].piece;
        }
    }
    return Piece(); // empty square
}

int asciiToCoord(char *inputStr)
{
    return (inputStr[0] >= 'a' && inputStr[0] <= 'h' &&
            inputStr[1] >= '1' && inputStr[1] <= '8') ?
        inputStr[0] - 'a' + ((inputStr[1] - '1') * 8) :
        FLAG;
}

// Direct a report to the user or the error log, whichever is more
// appropriate.  Always returns -1 (as a convenience).
int reportError(bool silent, const char *errorFormatStr, ...)
{
    char tmpBuf[160];
    va_list ap;

    va_start(ap, errorFormatStr);
    vsnprintf(tmpBuf, sizeof(tmpBuf), errorFormatStr, ap);
    va_end(ap);

    if (!silent)
    {
        gUI->notifyError(tmpBuf);
    }
    LOG_DEBUG("Error: %s\n", tmpBuf);
    return -1;
}


// Simple helper function.  Given a FEN fullmove and turn, return
// the appropriate ply.
static int fenFullmoveToPly(int fullmove, int turn)
{
    return (fullmove - 1) * 2 + turn;
}


// Sets 'board' to the FEN position described by 'fenString'.
// Returns -1 if invalid board detected (and board is left unaltered);
// 0 otherwise.
//
// We only accept standard FEN for an 8x8 board at this point.
int fenToBoard(const char *fenString, Board *result)
{
    int i, rank = 7, file = 0; // counters.
    int chr, spaces, res;
    Piece piece;
    Position tmpPosition;

    // Setting some defaults:
    // -- no one can castle
    // -- no en passant
    // -- white's turn
    cell_t epCoord = FLAG;
    int halfmove, fullmove;
    char coordStr[80], turnStr[6], cbyteStr[6], ebyteStr[6];

    if (fenString == NULL)
    {
        return reportError
            (false, "fenToBoard: NULL fenString (missing arg?)");
    }

    // Read in everything.  This is insensitive to whitespace between fields,
    // which is what we want since we want to work w/UCI.
    if ((res = sscanf(fenString, " %80s %6s %6s %6s %d %d",
                      coordStr, turnStr, cbyteStr, ebyteStr,
                      &halfmove, &fullmove)) < 6)
    {
        return reportError
            (false, "fenToBoard: not enough arguments (%d)", res);
    }

    // Handle the board pieces.
    for (i = 0;
         (chr = coordStr[i]) != '\0';
         i++)
    {
        if (isdigit(chr))
        {
            spaces = chr - '0';
            if (file + spaces > 8)
            {
                return reportError
                    (false, "fenToBoard: (%d,%d) too many spaces (%d)",
                     rank, file, spaces);
            }
            file += spaces;
        }
        else if (chr == '/')
        {
            if (file < 8 || rank <= 0)
            {
                return reportError
                    (false, "fenToBoard: (%d,%d) bad separator",
                     rank, file);
            }
            rank--;
            file = 0;
        }
        else if (!(piece = asciiToNative(chr)).IsEmpty())
        {
            if (file >= 8)
            {
                return reportError
                    (false, "fenToBoard: (%d,%d) too many pieces",
                     rank, file);
            }
            tmpPosition.SetPiece(ToCoord(rank, file++), piece);
        }
        else
        {
            // Unknown token.  Assume it's an unknown piece.
            return reportError
                (false, "fenToBoard: (%d,%d) unknown piece %c",
                 rank, file, chr);
        }
    }
    if (file != 8 || rank != 0)
    {
        return reportError
            (false, "fenToBoard: (%d,%d) bad terminator",
             rank, file);
    }

    // Read in turn.
    if (!strcmp(turnStr, "b"))
    {
        tmpPosition.SetTurn(1);
    }
    else if (strcmp(turnStr, "w"))
    {
        return reportError(false, "fenToBoard: unknown turn %s",
                           turnStr);
    }

    // Read in castling.
    if (strcmp(cbyteStr, "-")) // do nothing, if nothing to castle.
    {
        for (i = 0;
             i < 4 && (chr = cbyteStr[i]) != '\0';
             i++)
        {
            switch (chr)
            {
                case 'K':
                    tmpPosition.EnableCastlingOO(0);
                    break;
                case 'Q':
                    tmpPosition.EnableCastlingOOO(0);
                    break;
                case 'k':
                    tmpPosition.EnableCastlingOO(1);
                    break;
                case 'q':
                    tmpPosition.EnableCastlingOOO(1);
                    break;
                default:
                    return reportError
                        (false, "fenToBoard: unknown cbyte token '%c'", chr);
            }
        }
        if (i == 4 && cbyteStr[i] != '\0')
        {
            return reportError(false,
                               "fenToBoard: cbyteStr too long (%c)",
                               cbyteStr[i]);
        }
    }

    // Read in ebyte.
    if (strcmp(ebyteStr, "-"))
    {
        if ((epCoord = asciiToCoord(ebyteStr)) == FLAG)
        {
            return reportError(false, "fenToBoard: bad ebyte");
        }
        tmpPosition.SetEnPassantCoord(epCoord);
        if (ebyteStr[2] != '\0')
        {
            return reportError(false,
                               "fenToBoard: ebyteStr too long (%c)",
                               ebyteStr[2]);
        }
    }

    // Set fullmove and halfmove clock.
    if (!tmpPosition.SetPly(fenFullmoveToPly(fullmove, tmpPosition.Turn())) ||
        !tmpPosition.SetNcpPlies(halfmove))
    {
        return reportError(false,
                           "fenToBoard: bad fullmove/halfmove %d/%d",
                           fullmove, halfmove);
    }

    std::string errString;
    if (!tmpPosition.IsLegal(errString))
    {
        return reportError(false,
                           "fenToBoard: illegal position: %s",
                           errString.c_str());
    }

    // At this point we should have something good.
    result->SetPosition(tmpPosition);
    
    // result->ToPosition().Log(eLogEmerg);
    return 0;
}

bool isMove(const char *inputStr)
{
    char moveStr[MOVE_STRING_MAX];
    
    if (copyToken(moveStr, sizeof(moveStr), inputStr) == NULL)
        return false;
    
    if (!strcasecmp(moveStr, "O-O") || !strcmp(moveStr, "0-0") ||
        !strcasecmp(moveStr, "O-O-O") || !strcmp(moveStr, "0-0-0"))
    {
        return true;
    }

    int len = strlen(moveStr);
    return
        (len == 4 || len == 5) &&
        asciiToCoord(moveStr) != FLAG && asciiToCoord(&moveStr[2]) != FLAG &&
        (len == 4 || !asciiToNative(moveStr[4]).IsEmpty());
}

bool isLegalMove(const char *inputStr, MoveT *resultMove, const Board *board)
{
    if (resultMove != NULL)
        *resultMove = MoveNone; // preset this in case of failure
    if (board == NULL || !isMove(inputStr))
        return false;

    char moveStr[MOVE_STRING_MAX];
    copyToken(moveStr, sizeof(moveStr), inputStr);
    MoveT myMove = MoveNone;
    bool isNormalMove = false;
    
    if (!strcasecmp(moveStr, "O-O") || !strcmp(moveStr, "0-0"))
    {
        myMove.CreateFromCastle(true, board->Turn());
    }
    else if (!strcasecmp(moveStr, "O-O-O") || !strcmp(moveStr, "0-0-0"))
    {
        myMove.CreateFromCastle(false, board->Turn());
    }
    else
    {
        myMove.src = asciiToCoord(moveStr);
        myMove.dst = asciiToCoord(&moveStr[2]);
        myMove.UnmangleCastle(*board); // convert k2 or kxr moves
        myMove.promote = asciiToNative(moveStr[4]).Type();
        isNormalMove = true;
    }

    // Things could be slightly simpler here, except that we must take our en
    //  passant format into account; 'myMove' cannot get that correct.
    MoveList moveList;
    board->GenerateLegalMoves(moveList, false);

    const MoveT *foundMove = moveList.SearchSrcDst(myMove);
    if (foundMove == NULL)
        return false;

    if (foundMove->IsPromote())
    {
        if ((foundMove = moveList.SearchSrcDstPromote(myMove)) == NULL)
            return false;        
    }
    else if (isNormalMove && moveStr[4] != '\0')
    {
        return false; // do not allow trailing junk in the move
    }
    if (resultMove != NULL)
        *resultMove = *foundMove;
    return true;
}

// Like fgets(), but returns on any newline char, not just '\n'.
// The semantics of what an error is ("size" too small) might be unique, though.
static char *myFgets(char *s, int size, FILE *stream)
{
    int i = 0;
    int chr = EOF; // cannot assign directly to s[i] since EOF may be -1 and
                   // "char" may be unsigned; 255 != -1
    if (size < 1)
        return NULL;
    while (i < size - 1)
    {
        if ((chr = fgetc(stream)) == EOF ||
            isNewLineChar((s[i++] = chr)))
        {
            break;
        }
    }
    s[i] = '\0';
    return (chr == EOF && i == 0) ? NULL : s;
}

char *getStdinLine(int maxLen, Switcher *sw)
{
    // Polyglot likes to send long "position" commands (the startpos and all
    // the moves, not just the FEN position after the last capture/pawn push.
    // You can theoretically play a large number of moves in even a normal
    // chess game, especially considering the fact that the 50-move draw is
    // claimed, not automatic.
    // So, playing it safe here w/an expandable buffer.  We could go even more
    // complicated and read/parse only bits of input at a time, if desired.
    static char *buf = NULL;
    static int bufLen = 0;

    int bytesRead = 0; // for the current line

    while (bytesRead == 0 ||
           // expect line to be terminated w/a newline.  Otherwise, fgets()
           // ran out of room.
           !isNewLineChar(buf[bytesRead - 1]))
    {
        if (maxLen > 0 && maxLen < bytesRead)
        {
            reportError(false, "%s: maxLen exceeded, buffer was '%s'",
                        __func__, buf);
            exit(0);
        }
        if (bytesRead >= bufLen - 1 &&
            (buf = (char *) realloc(buf, (bufLen += 100))) == NULL)
        {
            reportError(false, "%s: could not alloc %d bytes\n",
                        __func__, bufLen + 100);
            exit(0);
        }
        if (myFgets(&buf[bytesRead], bufLen - bytesRead, stdin) == NULL)
        {
            reportError(false, "%s: fgets error %d, bailing\n",
                        __func__, errno);
            exit(0);
        }
        bytesRead += strlen(&buf[bytesRead]);
        if (bytesRead == 1 && isNewLineChar(buf[0]))
        {
            // read stand-alone newline.  Just discard it, switch off, and
            // come back when we have more input.
            // The reason for these shenanigans is, if a windows program sends
            // us CRLF, we will hit this.  We could avoid this with fgets(),
            // but that would screw up when handling a single Mac-style CR
            // (0x0d) which is explicitly allowed by the UCI spec.
            bytesRead = 0;
            sw->Switch();
        }
    }

    return buf;
}

// The point behind configuring these items before doing a newGame() is that
//  most engines (hopefully) won't allocate memory/threads until NewGame() is
//  called on them; so we avoid a re-allocation or possibly an initial over-
//  allocation.
void uiPrepareEngines(Game *game)
{
    int64 requestedMem = gPreCalc.userSpecifiedHashSize;
    int requestedNumThreads = gPreCalc.userSpecifiedNumThreads;
    
    if (requestedMem != -1)
    {
        game->EngineConfig().SetSpinClamped(Config::MaxMemorySpin,
                                            requestedMem / (1024 * 1024));
    }
    if (requestedNumThreads != -1)
    {
        game->EngineConfig().SetSpinClamped(Config::MaxThreadsSpin,
                                            requestedNumThreads);
    }
    game->NewGame();
}

static void uiThread(Game *game, Switcher *sw, Semaphore *readySem)
{
    gUI->init(game, sw);
    // Let the main thread know it is safe to continue.
    readySem->post();
    sw->Register();
    while (true)
        gUI->playerMove();
}

void uiThreadInit(Game *game, Switcher *sw, Semaphore *readySem)
{
    std::thread *mainUiThread =
        new std::thread(uiThread, game, sw, readySem);
    mainUiThread->detach();
}
