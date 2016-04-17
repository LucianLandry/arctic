//--------------------------------------------------------------------------
//                 MoveList.cpp - MoveList-oriented functions.
//                           -------------------
//  copyright            : (C) 2011 by Lucian Landry
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

#include "Board.h"
#include "gDynamic.h"
#include "MoveList.h"

//--------------------------------------------------------------------------
//                       PRIVATE CLASS DECLARATIONS:
//--------------------------------------------------------------------------

namespace // start unnamed namespace
{

// According to http://en.cppreference.com/w/cpp/utility/program/exit,
//  thread-local destructors will always run before any static destructors.
// That means the main thread will crash if it tries to push to the free list
//  in a static destructor.
// Of course the question remains ... is the storage space still there?
// According to:
//  http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2659.htm
// I am guessing it is, since "The address of a thread variable may be freely
//  used during the variable's lifetime by any thread in the program".
class FreeMoves
{
public:
    bool exiting;
    std::vector<std::vector<MoveT> *> freeMoves;
    FreeMoves() : exiting(false) {}
    ~FreeMoves() { exiting = true; }
};

} // end unnamed namespace
    
//--------------------------------------------------------------------------
//                       PRIVATE FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------

static inline bool historyWindowHit(const Board &board, cell_t from, cell_t to)
{
    return
        abs(gVars.hist[board.Turn()] [from] [to] - board.Ply()) < gVars.hiswin;
}

// Does not take promotion, en passant, or castling into account.
static inline bool isPreferredMoveFast(MoveT move, const Board &board)
{
    return !board.PieceAt(move.dst).IsEmpty() || move.chk != FLAG ||
        historyWindowHit(board, move.src, move.dst);
}

static inline bool isPreferredMove(MoveT move, const Board &board)
{
    return (move.src != move.dst && !board.PieceAt(move.dst).IsEmpty()) ||
        move.chk != FLAG || move.promote != PieceType::Empty ||
        historyWindowHit(board, move.src, move.dst);
}

static thread_local FreeMoves gFreeMoves;

static inline std::vector<MoveT> *allocMoves()
{
    std::vector<MoveT> *result;

    // It is not currently anticipated that the constructor will run
    // after thread-local variables have been destroyed, so we skip that check.
    if (!gFreeMoves.freeMoves.empty())
    {
        result = gFreeMoves.freeMoves.back();
        gFreeMoves.freeMoves.pop_back();
    }
    else
    {
        result = new std::vector<MoveT>;
        // let us assume we never have an empty(-capacity) vector.
        result->reserve(1);
    }

    return result;
}

//--------------------------------------------------------------------------
//                       PUBLIC FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------
MoveList::MoveList() : moves(*allocMoves())
{
    DeleteAllMoves();
}

MoveList::~MoveList()
{
    // Recycle our 'moves' vectors for later use, to prevent excess allocations.
    // We limit the pool size to prevent possible cross-thread memory leaks.
    if (!gFreeMoves.exiting && gFreeMoves.freeMoves.size() <= 100)
        gFreeMoves.freeMoves.push_back(&moves);
    else
        delete &moves;
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
void MoveList::UseAsFirstMove(MoveT firstMove)
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

MoveT *MoveList::SearchSrcDst(MoveT move)
{
    auto end = moves.end();

    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move.src == iter->src && move.dst == iter->dst)
        {
            return &(*iter);
        }
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
        {
            return &(*iter);
        }
    }

    return nullptr; // move not found.
}

MoveT *MoveList::Search(MoveT move)
{
    auto end = moves.end();
    
    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move == *iter)
        {
            return &(*iter);
        }
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

// Delete the move at index 'idx'.
void MoveList::DeleteMove(int idx)
{
    MoveT *move = &moves[idx];

    // I used 'insrt - 1' here but really we should always decrement
    // insrt for any preferred moves.
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

void MoveList::Log(LogLevelT level)
{
    char tmpStr[MOVE_STRING_MAX];
    const MoveStyleT style = {mnCAN, csK2, true};

    if (level > LogLevel())
	return; // no-op.

    LogPrint(level, "{mvlist NumMoves %d insrt %d ",
             // Log the private member variables.
	     NumMoves(), insrt);

    for (int i = 0; i < NumMoves(); i++)
	LogPrint(level, "%s ", MoveToString(tmpStr, moves[i], &style, NULL));

    LogPrint(level, "}\n");
}
