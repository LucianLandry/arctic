//--------------------------------------------------------------------------
//             BoardComponent.cpp - UI representation of a board.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
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

#include "aTypes.h"
#include "gPreCalc.h"

#include "BoardComponent.h"

using namespace juce;

BoardComponent::BoardComponent()
{
    resized();

    // Taken from the wikipedia light and dark background squares.
    Colour light(0xffffce9e), dark(0xffd18b47);

    for (int i = 0; i < NUM_SQUARES; i++)
    {
	squares[i].setColour(0,
			     (File(i) + Rank(i)) & 1 ?
			     light : dark);
	addAndMakeVisible(&squares[i]);
    }

    // Give the squares pieces.
    BoardT myBoard;
    BoardSet(&myBoard, gPreCalc.normalStartingPieces, CASTLEALL, FLAG, 0, 0,
	     0);
    refresh(&myBoard);
};

BoardComponent::~BoardComponent()
{
}

void BoardComponent::resized()
{
    Component::resized();
    for (int i = 0; i < NUM_SQUARES; i++)
    {
	squares[i].setBoundsRelative(File(i) / 8.0f,
				     (7.0f - Rank(i)) / 8.0f,
				     1.0f / 8.0f,
				     1.0f / 8.0f);
    }    
}

int BoardComponent::Rank(int i)
{
    return i >> 3;
}

int BoardComponent::File(int i)
{
    return (i & 7);
}

void BoardComponent::refresh(BoardT *board)
{
    for (int i = 0; i < NUM_SQUARES; i++)
    {
	squares[i].setPieceType(board->coord[i]);
    }
}
