//--------------------------------------------------------------------------
//                 MoveList.cpp - MoveList-oriented functions.
//                           -------------------
//  copyright            : (C) 2011 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "Board.h"
#include "HistoryWindow.h"
#include "MoveList.h"
#include "ObjectCache.h"

//--------------------------------------------------------------------------
//                       PRIVATE FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------

// Does not take promotion, en passant, or castling into account.
static inline bool isPreferredMoveFast(MoveT move, const Board &board)
{
    return !board.PieceAt(move.dst).IsEmpty() || move.chk != FLAG ||
        gHistoryWindow.Hit(move, board.Turn(), board.Ply());
}

static inline bool isPreferredMove(MoveT move, const Board &board)
{
    return (!move.IsCastle() && !board.PieceAt(move.dst).IsEmpty()) ||
        move.chk != FLAG || move.promote != PieceType::Empty ||
        gHistoryWindow.Hit(move, board.Turn(), board.Ply());
}

static thread_local ObjectCache<std::vector<MoveT>, 100> gFreeMoves;

//--------------------------------------------------------------------------
//                       PUBLIC FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------
MoveList::MoveList() : moves(*gFreeMoves.Alloc())
{
    DeleteAllMoves();
}

MoveList::~MoveList()
{
    // Recycle our 'moves' vectors for later use, to prevent excess allocations.
    gFreeMoves.Free(&moves);
}

const MoveList &MoveList::operator=(const MoveList &other)
{
    insrt = other.insrt;
    moves = other.moves;
    return *this;
}

void MoveList::SortByCapWorth(const Board &board)
{
    int i, j, besti;
    int maxWorth, myWorth;
    int cwArray[insrt]; // relies on variable-length arrays.  (sorry)

    for (i = 0; i < insrt; i++)
        cwArray[i] = board.CalcCapWorth(moves[i]);

    // Perform selection sort (by capture worth).  I don't bother caching
    //  capture worth, but if it shows up badly on gprof I can try it.
    for (i = 0; i < insrt - 1; i++)
    {
        // Find the best-worth move...
        for (j = i, besti = i, maxWorth = 0;
             j < insrt;
             j++)
        {
            myWorth = cwArray[j];
            if (myWorth > maxWorth)
            {
                maxWorth = myWorth;
                besti = j;
            }
        }

        // ... and if it's not the first move, swap w/it.
        if (besti != i)
        {
            std::swap(moves[i], moves[besti]);
            std::swap(cwArray[i], cwArray[besti]);
        }
    }
}

// In current profiles, this needs to be fast, so the code is pointerrific.
void MoveList::useAsFirstMove(MoveT firstMove)
{
    MoveT *foundMove = SearchSrcDstPromote(firstMove);

    if (foundMove == nullptr)
        return; // We have a missing or non-sensical move; just bail.

    MoveT myMove = *foundMove; // save off the found move

    MoveT *startMove = &moves[0];
    MoveT *firstNonPreferredMove = &moves[insrt];

    if (foundMove >= firstNonPreferredMove)
    {
        // This was a non-preferred move.  Move the 1st non-preferred
        // move into its spot.
        *foundMove = *firstNonPreferredMove;

        // Move the 1st move to (what will be) the last preferred move.
        *firstNonPreferredMove = *startMove;
        insrt++;
    }
    else
    {
        // This move was preferred.  Move the first move into its spot.
        *foundMove = *startMove;
    }

    *startMove = myMove; // Now replace the first move.
}

const MoveT *MoveList::SearchSrcDst(MoveT move) const
{
    auto end = moves.end();

    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move.src == iter->src && move.dst == iter->dst)
            return &(*iter);
    }

    return nullptr; // move not found.
}

MoveT *MoveList::SearchSrcDstPromote(MoveT move)
{
    auto end = moves.end();

    // The point of clobbering this is to be able to do a faster comparison
    //  check.
    move.chk = 0;

    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        MoveT tmpMove = *iter;
        tmpMove.chk = 0;
        
        if (move == tmpMove)
            return &(*iter);
    }

    return nullptr; // move not found.
}

const MoveT *MoveList::Search(MoveT move) const
{
    auto end = moves.end();
    
    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move == *iter)
            return &(*iter);
    }

    return nullptr; // move not found.
}

// Does not handle special cases of promotion, castling, or en passant.
// There is room for further optimization here during quiescing, because
//  all our moves are "preferred".
void MoveList::AddMoveFast(MoveT move, const Board &board)
{
    // prefetching &move.back() + 1 for a write doesn't seem to help here.
    if (isPreferredMoveFast(move, board))
    {
        // capture, check, or history move w/ depth?  Want good spot.
        if (int(moves.size()) == insrt)
        {
            moves.push_back(move);
            insrt++;
        }
        else
        {
            moves.push_back(moves[insrt]);
            moves[insrt++] = move;
        }
    }
    else
    {
        moves.push_back(move);
    }
}

// A slightly slower version of the above that takes the possibility of
// promotion, castling, and en passant into consideration.
void MoveList::AddMove(MoveT move, const Board &board)
{
    if (isPreferredMove(move, board))
    {
        // capture, promo, check, or history move w/ depth?  Want good spot.
        if (int(moves.size()) == insrt)
        {
            moves.push_back(move);
            insrt++;
        }
        else
        {
            moves.push_back(moves[insrt]);
            moves[insrt++] = move;
        }
    }
    else
    {
        moves.push_back(move);
    }
}

void MoveList::DeleteMove(int idx)
{
    MoveT *move = &moves[idx];

    if (IsPreferredMove(idx))
    {
        // Copy the last preferred move over this move (may be same move).
        *move = moves[--insrt];
        
        moves[insrt] = moves.back();
        moves.pop_back();
    }
    else
    {
        *move = moves.back(); // (may copy the move over itself)
        moves.pop_back();
    }
}

void MoveList::Log(LogLevelT level) const
{
    char tmpStr[MOVE_STRING_MAX];
    const MoveStyleT style = {mnCAN, csK2, true};

    if (level > LogLevel())
	return; // no-op.

    LogPrint(level, "{(MoveList) NumMoves %d insrt %d ",
             // Log the private member variables.
	     NumMoves(), insrt);

    for (int i = 0; i < NumMoves(); i++)
	LogPrint(level, "%s ", moves[i].ToString(tmpStr, &style, NULL));

    LogPrint(level, "}\n");
}
