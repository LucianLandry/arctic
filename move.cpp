//--------------------------------------------------------------------------
//                      move.c - chess moves for Arctic
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "Board.h"
#include "move.h"
#include "MoveList.h"
#include "uiUtil.h"
#include "Variant.h"

using arctic::File;
using arctic::Rank;

// Pull information about whose turn it is from this move.
// It only works for castling moves!
static uint8 moveCastleToTurn(MoveT move)
{
    return move.src & NUM_PLAYERS_MASK;
}

// Return whether a move *looks* sane, without knowing anything about whether
//  it is actually legal or not.
static bool moveIsSane(MoveT move)
{
    return
        move.src < NUM_SQUARES &&
        move.dst < NUM_SQUARES &&

        int(move.promote) >= 0 &&
        // This is a pretty twisted way to get around the lack of
        // kMaxPieceTypes; maybe I should just add that.
        Piece(NUM_PLAYERS - 1, move.promote).ToIndex() <= kMaxPieces &&

        (move.chk == FLAG || move.chk == DOUBLE_CHECK ||
         move.chk < NUM_SQUARES) &&

        // Do not allow a "non-"move (unless we are castling)
        (move.src != move.dst ||
         (moveCastleToTurn(move) < NUM_PLAYERS &&
          (move.src >> NUM_PLAYERS_BITS) <= 1 &&
          move.promote == PieceType::Empty));
}

// Safely print a move that seems to make no sense.
static char *moveToStringInsane(char *result, MoveT move)
{
    if (move == MoveNone)
    {
        sprintf(result, "(none)");
    }
    else
    {
        sprintf(result, "(INS! %x.%x.%x.%x)",
                move.src, move.dst, int(move.promote), move.chk);
    }
    return result;
}

static char *moveToStringMnDebug(char *result, MoveT move)
{
    char promoString[2] =
        {(move.promote != PieceType::Empty ?
          nativeToAscii(Piece(1, move.promote)) :
          '\0'), '\0'};
    char chkString[3] = {0};

    if (move.chk == DOUBLE_CHECK)
    {
        chkString[0] = 'D';
    }
    else if (move.chk != FLAG)
    {
        chkString[0] = AsciiFile(move.chk);
        chkString[1] = AsciiRank(move.chk);
    }
    // (We just keep chkString blank when move.chk == FLAG, since that is
    //  the default.)

    if (move.IsCastle() &&
        // No real castle would fail this condition:
        !(move.src >> (NUM_PLAYERS_BITS + 1)))
    {
        sprintf(result, "%s.%s.%s",
                move.IsCastleOO() ? "O-O" : "O-O-O",
                promoString,
                chkString);
    }
    else
    {
        sprintf(result, "%c%c%c%c.%s.%s",
                AsciiFile(move.src),
                AsciiRank(move.src),
                AsciiFile(move.dst),
                AsciiRank(move.dst),
                promoString,
                chkString);
    }
    
    return result;
}


// Assumes castling has been mangled, and that the move is sane.
static int moveToStringMnCAN(char *result, MoveT move)
{
    char promoString[2] =
        {(move.IsPromote() ?
          (char) tolower(nativeToAscii(Piece(0, move.promote))) :
          '\0'), '\0'};
    return sprintf(result, "%c%c%c%c%s",
                   AsciiFile(move.src),
                   AsciiRank(move.src),
                   AsciiFile(move.dst),
                   AsciiRank(move.dst),
                   promoString);
}

// We basically allow this conversion if we could reliably 'unmangle' the same
//  move from a standard drag-and-drop UI.  The allowance is per-player, but we
//  must be able to castle in both directions.
static bool canUseK2Notation(CastleStartCoordsT start)
{
    // We could forbid this notation when the "rooks" were not on the same rank
    //  as the king (diagonal castling in some variant??), or when the rooks
    //  were on the same side of the king.
    // But when those scenarios are unambiguous, so we currently do not.
    // We might regret that later if it proves to be confusing or error-prone.

    cell_t
        rookOO = start.rookOO,   // these are all shorthand.
        rookOOO = start.rookOOO,
        king = start.king;

    if (File(rookOO) == File(rookOOO)) // Degenerate, and ambiguous
        return false;

    if (File(rookOOO) > File(rookOO))
    {
        // force rookOO > rookOOO (simplifies algorithm; should not affect
        //  correctness)
        std::swap(rookOO, rookOOO);
    }

    return
        // Can only use king-moves-2 notation if the destination is on the same
        // rank.
        Rank(king) == Rank(king + 2) &&
        Rank(king) == Rank(king - 2) &&
        // Avoid situations where king-moves-2 could be confused with a
        //  conflicting KxR.
        king + 2 != rookOOO && king - 2 != rookOO;
}

// Attempt to transmute our normal castle style to a king-moves-2 style
// for printing.  Returns 'false' (and modifies nothing) if this is impossible.
static bool moveMangleCsK2(MoveT &move)
{
    CastleStartCoordsT start;
    bool castleOO = move.IsCastleOO();
    cell_t king, dst;

    start = Variant::Current()->Castling(moveCastleToTurn(move)).start;
    if (!canUseK2Notation(start))
        return false;

    king = start.king;

    if (File(start.rookOO) > File(start.rookOOO))
        dst = king + (castleOO ? 2 : -2);
    else
        dst = king + (castleOO ? -2 : 2);

    move.src = king;
    move.dst = dst;
    return true;
}

// Attempt to transmute our normal castle style to a king-capture-rook style
// for printing.
static void moveMangleCsKxR(MoveT &move)
{
    CastleStartCoordsT start;
    bool castleOO = move.IsCastleOO();
    cell_t king, dst;

    start = Variant::Current()->Castling(moveCastleToTurn(move)).start;
    dst = castleOO ? start.rookOO : start.rookOOO;
    king = start.king;

    move.src = king;
    move.dst = dst;
}

// Assumes castling is handled separately, when castleStyle is
//  csOO || csFIDE.  At this point we treat a king castle like
//  any other move even though it will not be technically legal.
static int moveToStringMnSAN(char *result, MoveT move, const Board &board)
{
    // See "https://en.wikipedia.org/wiki/Algebraic_notation_(chess)"
    //  for details about SAN, including move disambiguation.
    uint8 src = move.src;
    uint8 dst = move.dst;
    Piece myPiece = board.PieceAt(src);
    char *sanStr = result;
    int i;
    bool isCastle = move.IsCastle();
    bool isCapture = !isCastle &&
        (!board.PieceAt(dst).IsEmpty() || move.IsEnPassant());
    bool isPromote = move.IsPromote();
    bool isAmbiguous = false;
    bool ambiguousFile = false, ambiguousRank = false;
    MoveList mvlist;

    if (!myPiece.IsPawn())
        // Print piece (type) to move.
        sanStr += sprintf(sanStr, "%c", nativeToBoardAscii(myPiece));
    else if (isCapture)
        // Need to spew the file we are capturing from.
        sanStr += sprintf(sanStr, "%c", AsciiFile(src));

    board.GenerateLegalMoves(mvlist, false);

    // Is there ambiguity about which piece will be moved?
    for (i = 0; i < mvlist.NumMoves(); i++)
    {
        if (!myPiece.IsPawn() && // already taken care of, above
            mvlist.Moves(i).src != src &&
            mvlist.Moves(i).dst == dst &&
            board.PieceAt(mvlist.Moves(i).src) == myPiece)
        {
            isAmbiguous = true;
            if (!ambiguousFile)
                ambiguousFile = File(mvlist.Moves(i).src) == File(src);
            if (!ambiguousRank)
                ambiguousRank = Rank(mvlist.Moves(i).src) == Rank(src);
        }
    }

    // ... disambiguate the src piece, if necessary.
    if (isAmbiguous)
    {
        if (ambiguousFile && ambiguousRank)
            sanStr += sprintf(sanStr, "%c%c", AsciiFile(src), AsciiRank(src));
        else if (ambiguousFile)
            sanStr += sprintf(sanStr, "%c", AsciiRank(src));            
        else
            sanStr += sprintf(sanStr, "%c", AsciiFile(src));
    }
        
    if (isCapture)
        sanStr += sprintf(sanStr, "x");

    // spew the destination coord.
    sanStr += sprintf(sanStr, "%c%c", AsciiFile(dst), AsciiRank(dst));

    if (isPromote)
    {
        // spew piece type to promote to.
        sanStr += sprintf(sanStr, "%c",
                          nativeToBoardAscii(Piece(0, move.promote)));
    }
        
    return sanStr - result; // return number of non-NULL bytes written
}

char *MoveT::ToString(char *result,
                      const MoveStyleT *style,
                      // Used for disambiguation + legality checks, when !NULL.
                      const Board *board) const
{
    // These shorthand copies may be modified.
    MoveNotationT notation = style->notation;
    MoveCastleStyleT castleStyle = style->castleStyle;
    bool showCheck = style->showCheck;
    char *moveStr = result;
    
    if (!moveIsSane(*this))
    {
        // With our hashing scheme, we may end up with moves that are not
        //  legal, but we should never end up with moves that are not sane
        //  (except possibly MoveNone).
        // We still may want to print such a move before we assert (or
        //  whatever).
        return moveToStringInsane(result, *this);
    }
    if (board != nullptr && !board->IsLegalMove(*this))
    {
        result[0] = '\0';
        return result;
    }
    if (notation == mnDebug)
    {
        return moveToStringMnDebug(result, *this);
    }

    MoveT tmpMove(*this); // modifiable form of 'this'

    if (IsCastle())
    {
        // Transmute the move if we need to (and can); otherwise fall back to
        //  our default.
        if (castleStyle == csKxR)
            moveMangleCsKxR(tmpMove);
        else if (castleStyle == csK2 && !moveMangleCsK2(tmpMove))
            castleStyle = csOO;
    }

    if (tmpMove.IsCastle()) // (that is, a standard castle, not a mangled one)
    {
        if (castleStyle == csOO)
            moveStr += sprintf(moveStr, IsCastleOO() ? "O-O" : "O-O-O");
        else if (castleStyle == csFIDE)
            moveStr += sprintf(moveStr, IsCastleOO() ? "0-0" : "0-0-0");
    }
    else
    {
        // Cannot use SAN with no board context.
        if (notation == mnSAN && board == nullptr)
            notation = mnCAN;

        moveStr +=
            notation == mnSAN ? moveToStringMnSAN(result, tmpMove, *board) :
            // Assume mnCAN at this point.
            moveToStringMnCAN(result, tmpMove);
    }
    
    if (showCheck && chk != FLAG)
    {
        bool isMate = false;

        if (board != nullptr)
        {
            Board tmpBoard(*board);
            MoveList mvlist;

            // Piece in check.  Is this checkmate?
            tmpBoard.MakeMove(*this);
            tmpBoard.GenerateLegalMoves(mvlist, false);
            isMate = (mvlist.NumMoves() == 0);
        }

        moveStr += sprintf(moveStr, "%c", isMate ? '#' : '+');
    }

    return result;
}

// Writes out a sequence of moves using style 'moveStyle'.
// Returns the number of moves successfully converted.
int MovesToString(char *dstStr, int dstStrSize,
                  const MoveT *moves, int numMoves,
                  const MoveStyleT &moveStyle,
                  const Board &board)
{
    char sanStr[MOVE_STRING_MAX];
    Board tmpBoard(board);
    int movesWritten = 0;

    if (dstStr == nullptr || dstStrSize <= 0)
        return 0;

    dstStr[0] = '\0';
    int dstStrLen = 0;
    
    for (int i = 0; i < numMoves; i++)
    {
        moves[i].ToString(sanStr, &moveStyle, &tmpBoard);
        if (sanStr[0])
        {
            // Move was legal, advance to next move so we can check it.
            tmpBoard.MakeMove(moves[i]);
        }
        else
        {
            MoveStyleT badMoveStyle = {mnDebug, csOO, false};
            char tmpStr[MOVE_STRING_MAX];

            // Sanity check for illegal moves.
            // Shouldn't happen with a well-behaved engine.
            LogPrint(eLogNormal,
                     "%s: illegal move %s (%d/%d) baseply %d, ignoring\n",
                     __func__,
                     moves[i].ToString(tmpStr, &badMoveStyle, nullptr),
                     i, numMoves, board.Ply());
            break;
        }

        if (dstStrLen + strlen(sanStr) +
            (i == 0 ? 0 : 1) // account for leading space before move
            >= (size_t)dstStrSize)
        {
            // Not enough space in the result to write the next move.
            break;
        }
        
        // Build up the result string.
        dstStrLen += sprintf(&dstStr[dstStrLen], "%s%s",
                             // Do not use leading space before first move.
                             i == 0 ? "" : " ",
                             sanStr);
        
        assert(dstStrLen < dstStrSize);
        movesWritten++;
    }

    return movesWritten;
}

// This is only a partial move creation routine as it does not fill in
//  'chk', and in fact, clobbers it.
void MoveT::CreateFromCastle(bool castleOO, int turn)
{
    src = castleOO ? turn : (1 << NUM_PLAYERS_BITS) | turn;
    dst = src;
    promote = PieceType::Empty;
    chk = FLAG;
}

// Attempt to take a king-moves-2 or KxR-style move and convert it
//  to the correct format.  Does nothing if the move is not actually detected
//  as a castle.
// We need a 'board' arg (or at least cbyte) because otherwise (for example) a
//  king capturing its own rook one space to the right could be confused with
//  just moving the king one space to the right.
// Assumes we are 'unmangling' a move from the players whose turn it is.
void MoveT::UnmangleCastle(const Board &board)
{
    CastleStartCoordsT start; // shorthand
    cell_t rookOO, rookOOO;
    bool isCastleOO;
    int turn = board.Turn();

    if (IsCastle())
        return; // do not unmangle if this move is already a castle request

    start = Variant::Current()->Castling(turn).start;
    rookOO = start.rookOO;
    rookOOO = start.rookOOO;
    
    if (src != start.king || !board.CanCastle(turn))
        return;

    // We know now we're at least trying to move a 'king' that can castle.
    if ((dst == rookOO || dst == rookOOO) &&
        board.PieceAt(dst).Player() == turn)
    {
        // Attempting KxR (or at least Kx"something of its own color"; we are
        //  trying to be flexible here for possible variants.)
        isCastleOO = (dst == rookOO);
    }
    else if (abs(dst - src) == 2)
    {
        // Attempting K moves 2.
        if (!canUseK2Notation(start))
            return;

        isCastleOO =
            (File(rookOO) > File(rookOOO) && dst > src) ||
            (File(rookOO) < File(rookOOO) && dst < src);
    }
    else
    {
        return; // King not moving 2, and not capturing own rook
    }

    if ((isCastleOO && board.CanCastleOO(turn)) ||
        (!isCastleOO && board.CanCastleOOO(turn)))
    {
        CreateFromCastle(isCastleOO, turn);
    }
}
