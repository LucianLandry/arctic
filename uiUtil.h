//--------------------------------------------------------------------------
//                 uiUtil.h - UI-oriented utility functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef UIUTIL_H
#define UIUTIL_H

#include "aSemaphore.h"
#include "aTypes.h"
#include "Board.h"
#include "Game.h"
#include "Piece.h"
#include "ref.h"       // Rank(), File()
#include "Switcher.h"

// Piece conversion routines.
Piece asciiToNative(char ascii);
char nativeToAscii(Piece piece);
char nativeToBoardAscii(Piece piece);

static inline char AsciiFile(cell_t coord)
{
    return arctic::File(coord) + 'a';
}

static inline char AsciiRank(cell_t coord)
{
    return arctic::Rank(coord) + '1';
}

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
int fenToBoard(const char *fenString, Board *result);

// Direct a report to the user or the error log, whichever is more
// appropriate.  Always returns -1 (as a convenience).
int reportError(bool silent, const char *errorFormatStr, ...)
    __attribute__ ((format (printf, 2, 3)));

// Return whether or not the first token in 'inputStr' looks like a move.
// Currently we can only handle NUL-terminated, mnCAN-style moves (but all
//  castling styles).
bool isMove(const char *inputStr);

// Return whether or not the first token in 'inputStr' looks like a legal move.
// Currently we can only handle NUL-terminated, mnCAN-style moves (but all
//  castling styles).
// Side effect: fills in 'resultMove' (w/MoveNone if the move is illegal).
bool isLegalMove(const char *inputStr, MoveT *resultMove, const Board *board);

// Get a line of input from stdin, of max string length "maxLen"
// (or unlimited if maxLen <= 0)
char *getStdinLine(int maxLen, Switcher *sw);

void uiPrepareEngines(Game *game); // Tell the engines we are about to start
void uiThreadInit(Game *game, Switcher *sw,
                  arctic::Semaphore *readySem);

#endif // UIUTIL_H
