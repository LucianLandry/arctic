//--------------------------------------------------------------------------
//                      move.h - chess moves for Arctic
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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


#ifndef MOVE_H
#define MOVE_H

#include "aTypes.h"
#include "ref.h"

#ifdef __cplusplus
extern "C" {
#endif

// Our basic structure for representing a chess move.
// The normal convention will be to try to pass around MoveTs by structure
//  (not with pointers) because doing so should be cheaper (4 bytes vs 8 bytes
//  on a 64-bit arch).
//
// Castling moves follow a peculiar convention: 'src' =
//  "((isCastleOO ? 1 : 0) << NUM_PLAYERS_BITS) | turn"; and
//  'dst' = 'src'.
// The reasoning is that it is very easy to detect a castle (check src == dst),
//  and the notation is portable across chess variants.  We mask in 'turn'
//  because it becomes easier to convert back to other notations.

typedef struct {
    cell_t src;     // For a null/invalid move, src = FLAG (and the contents
                   //  of the other fields are undefined.)
    cell_t dst;
    uint8 promote; // Piece to promote pawn to (usually nothing, -> 0)
                   // Also (ab)used for en passant, signified by (B)PAWN of
                   //  opposite color (ie the color of the pawn we are going to
                   //  capture).

    cell_t chk;    // Is this a checking move.  Set to:
                   // FLAG if not a checking move.
                   // Coordinate of checking piece, if single check
                   // DOUBLE_CHECK otherwise.
} MoveT;

// Any stringified-move (including null terminator) is guaranteed to fit into
// a char[MOVE_STRING_MAX].
#define MOVE_STRING_MAX (20) // Need this length for insane strings (rounded
                             //  up to a 4-byte boundary)

// Various ways to represent a move as a string.
// See http://en.wikipedia.org/wiki/Chess_notation
typedef enum {
    mnSAN,  // Standard algebraic notation (most human-readable.  Example:
            //  "bxa8Q")

    mnCAN,  // Coordinate albebraic notation (but no dashes, no parenthesis for
            //  promotion.  Example: "b7a8q")
    
    mnDebug // Stringified representation of full MoveT structure, used for
            //  debugging.  Example: "b7a8.12.FF".  Ignores castleStyle and
            //  showCheck.
} MoveNotationT;

typedef enum {
    csOO,     // Use (PGN) "O-O" and "O-O-O" (even for nCAN).  This is our
              //  preferred internal string representation.
    csFIDE,   // Like above, but use zeros ("0-0") instead of letter Os.
              //  Currently unneeded, but trivial to implement.
    csKxR,    // Use King-captures-rook notation (used by UCI for chess960)
    csK2      // Use King-moves-2-spaces notation (falls back to csOO when this
              //  is impossible)
} MoveCastleStyleT;

// Obviously the code implements a limited range of move styles.  It can be
// expanded if necessary.
typedef struct {
    MoveNotationT notation;
    MoveCastleStyleT castleStyle;
    bool showCheck; // Append '+' and '#' (when known) to moves?
} MoveStyleT;

struct BoardS; // forward declaration

// "No" move (fails moveIsSane(), so do not try to print it)
extern const MoveT gMoveNone;

// Syntactic sugar.
void MoveStyleSet(MoveStyleT *style,
		  MoveNotationT notation,
		  MoveCastleStyleT castleStyle,
		  bool showCheck);

char *MoveToString(char *result,
		   MoveT move,
		   const MoveStyleT *style,
		   // Used for disambiguation and legality checks, when !NULL.
		   // Not mangled, but may be altered (to test checkmate).
		   struct BoardS *board);

static inline bool MoveIsCastle(MoveT move)
{
    return move.src == move.dst;
}

bool MoveIsPromote(MoveT move, struct BoardS *board);

static inline bool MoveIsCastleOO(MoveT move)
{
    return MoveIsCastle(move) && (move.src >> NUM_PLAYERS_BITS) == 0;
}

static inline bool MoveIsCastleOOO(MoveT move)
{
    return MoveIsCastle(move) && (move.src >> NUM_PLAYERS_BITS) == 1;
}

// This is only a partial move creation routine as it does not fill in
//  'move->chk', and in fact, clobbers it.
void MoveCreateFromCastle(MoveT *move, bool castleOO, int turn);

// Attempt to take a king-moves-2 or KxR-style move and convert it
//  to the correct format.  Does nothing if the move is not actually detected
//  as a castle.
// We need a 'board' arg (or at least cbyte) because otherwise (for example) a
//  king capturing its own rook one space to the right could be confused with
//  just moving the king one space to the right.
// Assumes we are 'unmangling' a move from the players whose turn it is.
void MoveUnmangleCastle(MoveT *move, struct BoardS *board);

#ifdef __cplusplus
}
#endif

#endif // MOVE_H
