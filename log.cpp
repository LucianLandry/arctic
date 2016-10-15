//--------------------------------------------------------------------------
//                     log.cpp - debugging log support.
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
#include <stdarg.h>
#include <stdio.h>

#include "aSystem.h"
#include "log.h"
#include "uiUtil.h"

using arctic::ToCoord;

LogLevelT gLogLevel = eLogNormal;
static FILE *gLogFile = NULL;
const MoveStyleT gMoveStyleLog = {mnCAN, csK2, true};

void LogInit(void)
{
    std::string logName = SystemAppDirectory();
    assert(logName != "");
    logName += "/errlog";
    gLogFile = fopen(logName.c_str(), "w");
    if (gLogFile == NULL)
    {
        fprintf(stderr, "cannot open %s for writing (logging disabled)\n",
                logName.c_str());
        gLogFile = fopen(SystemNullFile().c_str(), "w");
        if (gLogFile == NULL)
        {
            fprintf(stderr, "cannot open null file for writing\n");
            assert(0);
        }
    }
    // turn off buffering -- useful when we're crashing, but slow.
    setbuf(gLogFile, NULL);
    // LogSetLevel(eLogDebug);
}

void LogSetLevel(LogLevelT level)
{
    gLogLevel = level;
}

void LogFlush(void)
{
    if (gLogFile) fflush(gLogFile);
}

int LogPrint(LogLevelT level, const char *format, ...)
{
    va_list ap;
    int rv = 0;

    if (level <= gLogLevel)
    {
        va_start(ap, format);
        rv = vfprintf(gLogFile ? gLogFile : stderr, format, ap);
        va_end(ap);
    }

    // Not sure if it is 'proper' to do this or not, but it sure seems
    //  convenient.
    if (level == eLogEmerg)
        LogFlush();
        
    return rv;
}

// debugging funcs.
void LogMove(LogLevelT level, const Board *board, MoveT move, int searchDepth)
{
    int moveDepth;
    Piece capPiece;
    char capstr[6];
    char promostr[6];
    char chkstr[10];
    char levelstr[160];
    char *myLevelstr;
    char tmpStr[MOVE_STRING_MAX];

    if (level > gLogLevel)
    {
        return; // no-op
    }

    // optimization: do all initialization after gLogLevel check.
    myLevelstr = levelstr;
    capPiece = board->PieceAt(move.dst);
    capstr[0] = '\0';
    promostr[0] = '\0';
    chkstr[0] = '\0';

    myLevelstr += sprintf(myLevelstr, "D%02d", searchDepth);
    for (moveDepth = MIN(searchDepth, 20); moveDepth > 0; moveDepth--)
    {
        myLevelstr += sprintf(myLevelstr, "    ");
    }
    if (!capPiece.IsEmpty())
    {
        sprintf(capstr, "(x%c)", nativeToAscii(capPiece));
    }
    if (move.IsPromote())
    {
        sprintf(promostr, "(->%c)", nativeToAscii(Piece(0, move.promote)));
    }
    if (move.chk != FLAG)
    {
        sprintf(chkstr, "(chk-%c%c)",
                AsciiFile(move.chk), AsciiRank(move.chk));
    }
    LogPrint(level, "%s%s%s%s%s\n",
             levelstr,
             move.ToString(tmpStr, &gMoveStyleLog, NULL),
             capstr, promostr, chkstr);
}

void LogMoveShow(LogLevelT level, const Board *board, MoveT move, const char *caption)
{
    int ascii, i, j;
    char tmpStr[MOVE_STRING_MAX];

    LogPrint(level, "%s:\nMove was %s\n", caption,
             move.ToString(tmpStr, &gMoveStyleLog, NULL));

    for (i = 7; i >= 0; i--)
    {
        for (j = 0; j < 8; j++)
        {
            ascii = nativeToAscii(board->PieceAt(ToCoord(i, j)));
            LogPrint(level, "%c", ascii == ' ' ? '.' : ascii);
        }
        LogPrint(level, "\n");
    }
}
