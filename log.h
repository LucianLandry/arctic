//--------------------------------------------------------------------------
//                      log.h - debugging log support.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef LOG_H
#define LOG_H

#include "aTypes.h"
#include "move.h"

// Forward declaration of classes that depend on us.
class Position;
class Board;

typedef enum {
    eLogEmerg,
    eLogNormal,
    eLogDebug
} LogLevelT;

// private, should not be used by outside modules.  Use LogLevel() instead.
extern LogLevelT gLogLevel;

void LogInit(void);
static inline LogLevelT LogLevel(void);
void LogSetLevel(LogLevelT level);
void LogFlush(void);
int LogPrint(LogLevelT level, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void LogMoveShow(LogLevelT level, const Board *board, MoveT move, const char *caption);
void LogMove(LogLevelT level, const Board *board, MoveT move, int searchDepth);

#define LOG_EMERG(format, ...) LogPrint(eLogEmerg, (format), ##__VA_ARGS__)

#define LOG_NORMAL(format, ...) \
    do \
        if (gLogLevel >= eLogNormal) \
        { \
            LogPrint(eLogNormal, (format), ##__VA_ARGS__); \
        } \
    while (0)

// These macros are normally disabled because checking the log level while
// "thinking" gives a small, but noticable hit on performance.
#ifdef ENABLE_DEBUG_LOGGING
#define LOG_DEBUG(format, ...) \
    do \
        if (gLogLevel >= eLogDebug) \
        { \
            LogPrint(eLogDebug, (format), ##__VA_ARGS__); \
        } \
    while (0)

#define LOGMOVE_DEBUG(board, move, searchDepth) \
    LogMove(eLogDebug, (board), (move), (searchDepth))

#else // !ENABLE_DEBUG_LOGGING

#define LOG_DEBUG(format, ...)
#define LOGMOVE_DEBUG(board, move, searchDepth)
#define LOGMOVELIST_DEBUG(mvlist)

#endif // ifdef ENABLE_DEBUG_LOGGING

static inline LogLevelT LogLevel(void)
{
    return gLogLevel;
}
    
#endif // LOG_H
