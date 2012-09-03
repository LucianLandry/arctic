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

// Piece conversion routines.
int asciiToNative(uint8 ascii);
int nativeToAscii(uint8 piece);
int nativeToBoardAscii(uint8 piece);

#define AsciiFile(coord) (File(coord) + 'a')
#define AsciiRank(coord) (Rank(coord) + '1')

// Given input like 'a1', returns something like '0'
// (or FLAG, if not a sensible coord)
int asciiToCoord(char *inputStr);

// Converts a move into (extended) human-readable format.
// Returns 'result' (should be at least 6 bytes in length).
char *moveToStr(char *result, MoveT *move);
// Returns 'result' (should be at least 11 bytes in length).
char *moveToFullStr(char *result, MoveT *move);

// Writes out moves in SAN (Nf3) if 'useSan' == true,
// otherwise long algebraic notation (g1f3) is used.
// Iff 'chopFirst' is true, the first move is not written (but still checked for
// legality).
// Returns the number of moves successfully converted.
int buildMoveString(char *dstStr, int dstLen, PvT *pv, BoardT *board,
		    bool useSan, bool chopFirst);

#define FEN_STARTSTRING \
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// Sets 'board' to the FEN position described by 'fenString'.
// Returns -1 if invalid board detected (and board is left unaltered);
// 0 otherwise.
//
// We only accept standard FEN for an 8x8 board at this point.
int fenToBoard(char *fenString, BoardT *result);

// Direct a report to the user or the error log, whichever is more
// appropriate.  Always returns -1 (as a convenience).
int reportError(int silent, char *errorFormatStr, ...)
    __attribute__ ((format (printf, 2, 3)));

// Stop everything (including clocks) and wait for further input, basically.
void setForceMode(ThinkContextT *th, GameT *game);

// Return whether or not 'inputStr' looks like a move.
// NULL "inputStr"s are not moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation.
bool isMove(char *inputStr, MoveT *resultMove);

// Return whether or not 'inputStr' looks like a legal move.
// NULL "inputStr"s are not legal moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation.
bool isLegalMove(char *inputStr, MoveT *resultMove, BoardT *board);

// Pattern matchers for tokens embedded at the start of a larger string.
bool matches(char *str, char *needle);
bool matchesNoCase(char *str, char *needle);

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
