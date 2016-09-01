//--------------------------------------------------------------------------
//                   pv.cpp - preferred variation handling.
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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

#include <algorithm>  // std::copy, std::fill
#include <assert.h>

#include "Board.h"
#include "move.h" // MovesToString()
#include "Pv.h"

SearchPv &SearchPv::operator=(const SearchPv &other)
{
    if (this != &other)
    {
        startDepth = other.startDepth;
        numMoves = other.numMoves;
        std::copy(&other.moves[0], &other.moves[numMoves], moves);
    }
    return *this;
}

// As a convenience, returns true iff this is the root node.
bool SearchPv::updateFromChildPv(MoveT move, const SearchPv *child)
{
    if (child != nullptr && child->startDepth != startDepth + 1)
    {
        LOG_EMERG("%s: ERROR! startDepths wrong: mine %d, child %d\n",
                  __func__, startDepth, child->startDepth);
    }
    
    if (move == MoveNone)
    {
        // Once we have a move, we should never UpdatePv() with MoveNone.
        // (because MoveNone should only happen on a fail-low)
        if (numMoves > 0)
        {
            assert(0);
        }
        // Assuming we didn't assert, this case is a no-op (because numMoves
        //  already == 0).
    }
    // When startDepth >= kMaxPvMoves, it's useless to update, since this
    //  cannot percolate back to the root node.
    else if (startDepth < kMaxPvMoves)
    {
        moves[0] = move;

        if (child == nullptr)
        {
            numMoves = 1;
        }
        else
        {
            int movesToCopy = MIN(child->numMoves,
                                  kMaxPvMoves - 1 - startDepth);
            numMoves = movesToCopy + 1;
            if (movesToCopy > 0)
            {
                std::copy(&child->moves[0], &child->moves[movesToCopy],
                          &moves[1]);
            }
        }
    }

    return startDepth == 0;
}

// Writes out a sequence of moves in the PV using style 'moveStyle'.
// Returns the number of moves successfully converted.
int SearchPv::BuildMoveString(char *dstStr, int dstLen,
                              const MoveStyleT &moveStyle,
                              const Board &board) const
{
    int retVal = MovesToString(dstStr, dstLen, moves, numMoves, moveStyle, board);
    LOG_DEBUG("%s: returning string %s\n", __func__, dstStr);
    return retVal;
}

void SearchPv::Log(LogLevelT logLevel) const
{
    LogPrint(logLevel, "{(SearchPv) startDepth %d numMoves %d moves {",
             startDepth, numMoves);

    for (int i = 0; i < numMoves; i++)
    {
        char tmpStr[MOVE_STRING_MAX];
        // (the last 2 args are dontcares)
        const MoveStyleT style = {mnDebug, csOO, false};

        LogPrint(logLevel, "%s%s",
                 i ? " " : "",
                 moves[i].ToString(tmpStr, &style, nullptr));
    }
    LogPrint(logLevel, "}}");
}

void SearchPv::Decrement()
{
    // "++" instead of "--" since this is depth of node this was instantiated
    //  at, not 'level'
    startDepth++;
    if (numMoves <= 0)
        return;
    numMoves--;
    int i;
    for (i = 0; i < numMoves; i++)
        moves[i] = moves[i + 1];
    moves[i] = MoveNone;
}

void DisplayPv::Log(LogLevelT logLevel) const
{
    char tmpStr[kMaxEvalStringLen];
    LogPrint(logLevel, "{(DisplayPv) level %d eval %s pv ",
             level, eval.ToLogString(tmpStr));
    pv.Log(logLevel);
    LogPrint(logLevel, "}");
}

void DisplayPv::Decrement()
{
    pv.Decrement();
    level--; // I guess we'll allow this to go negative (into quiescing moves
             //  only)...
    eval.Invert().RipenFrom(Eval::WinThreshold);
}

HintPv::HintPv()
{
    Clear();
}

void HintPv::Clear()
{
    level = 0;
    eval.Set(Eval::Loss, Eval::Win);
    completedSearch = false;
    std::fill(&moves[0], &moves[kMaxPvMoves], MoveNone);
}

void HintPv::Update(const DisplayPv &dispPv)
{
    level = dispPv.Level();
    eval = dispPv.Eval();

    int numMoves = MIN(kMaxPvMoves, dispPv.Level() + 1);

    // We purposefully do not track quiescing moves because we do not
    //  want to be forced into a capture chain.  We preserve the rest of
    //  moves[] since it might be helpful to Hint().
    for (int i = 0; i < numMoves && dispPv.Moves(i) != MoveNone; i++)
        moves[i] = dispPv.Moves(i);

    completedSearch = false;
}

void HintPv::Decrement(MoveT move)
{
    bool bPredictedMove = move != MoveNone && move == moves[0];

    // Adjust the principal variation (shrink it by depth one after a move).
    // If we did not make the move the computer predicted, this can
    // result in nonsensical moves being kept around.  But we can still use
    // the PV as a hint as to what moves to prefer.
    std::copy(&moves[1], &moves[kMaxPvMoves], &moves[0]);
    moves[kMaxPvMoves - 1] = MoveNone;
    eval.Invert().RipenFrom(Eval::WinThreshold);
    
    // If we successfully predicted the move, we can start the next search
    // at the PV's level.
    level = bPredictedMove ? MAX(level - 1, 0) : 0;
}

// Preserve the future moves of a variation that may not come to pass.
void HintPv::Rewind(int numPlies)
{
    if (numPlies <= 0)
    {
        if (numPlies < 0)
            FastForward(-numPlies);
        return;
    }
    numPlies = MIN(kMaxPvMoves, numPlies);

    std::copy_backward(&moves[0], &moves[kMaxPvMoves - numPlies],
                       &moves[kMaxPvMoves]);
    std::fill(&moves[0], &moves[numPlies], MoveNone);

    // We need to clear everything else out, though, because it is no longer
    // valid since we have no idea what move might be selected.
    level = 0;
    eval.Set(Eval::Loss, Eval::Win);
}

void HintPv::FastForward(int numPlies)
{
    if (numPlies < 0)
    {
        Rewind(-numPlies);
        return;
    }
    for (numPlies = MIN(kMaxPvMoves, numPlies); numPlies > 0; numPlies--)
        Decrement(moves[0]);
}

int HintPv::SuggestSearchStartLevel()
{
    return
        // Always try to find the shortest mate if we have stumbled
        // onto one.  But normally we start at a deeper level just to
        // save the cycles.
        eval.DetectedWinOrLoss() ? 0 :
        // We start the search at the same level as the PV if we did not
        // complete the search, or at the next level if we did.  If the PV
        // level is zero, we just start over because the predicted move may
        // not have been made.  (An alternative to this hackery would be to
        // set level = -1 iff Decrement() didn't hit the right move.)
        completedSearch && level > 0 ? level + 1 :
        level;
}

void HintPv::ResetSearchStartLevel()
{
    level = 0;
}

void HintPv::CompletedSearch()
{
    completedSearch = true;
}

void HintPv::Log(LogLevelT logLevel) const
{
    if (logLevel > LogLevel())
        return; // no-op

    // (the last 2 args are dontcares)
    const MoveStyleT style = {mnDebug, csOO, false};

    LogPrint(logLevel, "{(HintPv %p) level %d moves {", this, level);

    bool printedFirstMove = false;
    for (int i = 0; i < kMaxPvMoves; i++)
    {
        char tmpStr[MOVE_STRING_MAX];

        if (moves[i] != MoveNone)
        {
            LogPrint(logLevel, "%s%d: %s",
                     printedFirstMove ? " " : "",
                     i,
                     moves[i].ToString(tmpStr, &style, nullptr));
            printedFirstMove = true;
        }
    }
    char tmpStr[kMaxEvalStringLen];
    LogPrint(logLevel, "} eval %s completedSearch %s}",
             eval.ToLogString(tmpStr),
             completedSearch ? "true" : "false");
}
