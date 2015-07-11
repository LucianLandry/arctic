//--------------------------------------------------------------------------
//                 uiUtil.h - UI-oriented utility functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef UIUTIL_H
#define UIUTIL_H

#include "aTypes.h"
#include "thinker.h"
#include "board.h"
#include "game.h"
#include "ref.h"       // Rank(), File()
#include "switcher.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline char AsciiFile(cell_t coord)
{
    return File(coord) + 'a';
}

static inline char AsciiRank(cell_t coord)
{
    return Rank(coord) + '1';
}

// Piece conversion routines.
int asciiToNative(char ascii);
char nativeToAscii(uint8 piece);
char nativeToBoardAscii(uint8 piece);

// Given input like 'a1', returns something like '0'
// (or FLAG, if not a sensible coord)
int asciiToCoord(char *inputStr);

#define FEN_STARTSTRING \
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// Sets 'board' to the FEN position described by 'fenString'.
// Returns -1 if invalid board detected (and board is left unaltered);
// 0 otherwise.
//
// We only accept standard FEN for an 8x8 board at this point.
int fenToBoard(const char *fenString, BoardT *result);

// Direct a report to the user or the error log, whichever is more
// appropriate.  Always returns -1 (as a convenience).
int reportError(int silent, const char *errorFormatStr, ...)
    __attribute__ ((format (printf, 2, 3)));

// Stop everything (including clocks) and wait for further input, basically.
void setForceMode(ThinkContextT *th, GameT *game);

// Token support routines.
char *findNextNonWhiteSpace(char *pStr);
char *findNextWhiteSpace(char *pStr);
char *findNextWhiteSpaceOrNull(char *pStr);

// Return whether or not 'inputStr' looks like a move.
// NULL "inputStr"s are not moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation.
bool isMove(char *inputStr, MoveT *resultMove, BoardT *board);

// Return whether or not 'inputStr' looks like a legal move.
// NULL "inputStr"s are not legal moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation.
bool isLegalMove(char *inputStr, MoveT *resultMove, BoardT *board);

// Pattern matchers for tokens embedded at the start of a larger string.
bool matches(const char *str, const char *needle);
bool matchesNoCase(const char *str, const char *needle);

// Get a line of input from stdin, of max string length "maxLen"
// (or unlimited if maxLen <= 0)
char *getStdinLine(int maxLen, SwitcherContextT *sw);

// Terminate a string 's' at the 1st occurence of newline.  Returns 's' as a
// convenience.
char *ChopBeforeNewLine(char *s);

void uiThreadInit(ThinkContextT *th, GameT *game);

#ifdef __cplusplus
}
#endif

#endif // UIUTIL_H
