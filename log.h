/***************************************************************************
                        log.h - debugging log support.
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#ifndef LOG_H
#define LOG_H

#include <stdio.h> /* "FILE *" */
#include "aTypes.h"
#include "board.h"
#include "moveList.h"

enum {
    eLogEmerg,
    eLogNormal,
    eLogDebug
} LogLevelT;

/* private, should not be used by outside modules. */
extern int gLogLevel;

void LogSetFile(FILE *logFile);
void LogSetLevel(int level);
void LogFlush(void);
int LogPrint(int level, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void LogMoveShow(int level, BoardT *board, MoveT *move, char *caption);
void LogPieceList(BoardT *board);

#define LOG_EMERG(format, ...) LogPrint(eLogEmerg, (format), ##__VA_ARGS__)

#ifdef ENABLE_DEBUG_LOGGING
void LogMoveList(int level, MoveListT *mvlist);
void LogMove(int level, BoardT *board, MoveT *move);
void LogBoard(int level, BoardT *board);
#define LOG_NORMAL(format, ...) \
    do \
        if (gLogLevel >= eLogNormal) \
        { \
            LogPrint(eLogNormal, (format), ##__VA_ARGS__); \
        } \
    while (0)
#define LOG_DEBUG(format, ...) \
    do \
        if (gLogLevel >= eLogDebug) \
        { \
            LogPrint(eLogDebug, (format), ##__VA_ARGS__); \
        } \
    while (0)
#else
#define LOG_NORMAL(format, ...)
#define LOG_DEBUG(format, ...)
static inline void LogMoveList(int level, MoveListT *mvlist) { }
static inline void LogMove(int level, BoardT *board, MoveT *move) { }
static inline void LogBoard(int level, BoardT *board) { }
#endif

#endif /* LOG_H */