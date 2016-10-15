//--------------------------------------------------------------------------
//             BoardComponent.cpp - UI representation of a board.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "aTypes.h"
#include "Variant.h"

#include "BoardComponent.h"

using arctic::File;
using arctic::Rank;
using juce::Colour;

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
    refresh(Variant::Current()->StartingPosition());
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

void BoardComponent::refresh(const Position &position)
{
    for (int i = 0; i < NUM_SQUARES; i++)
    {
        squares[i].SetPiece(position.PieceAt(i));
    }
}
