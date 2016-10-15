//--------------------------------------------------------------------------
//          SquareComponent.cpp - UI representation of a board square.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <iostream>

#include "PieceCache.h"
#include "SquareComponent.h"

using namespace juce;

SquareComponent::SquareComponent() : backgroundColour(Colours::black)
{
}

SquareComponent::~SquareComponent()
{
}

void SquareComponent::paint(Graphics &g)
{
    g.fillAll(findColour(0));
}

void SquareComponent::colourChanged()
{
    repaint();
}

void SquareComponent::transformPiece()
{
    if (piecePicture != nullptr)
    {
#if 0
        piecePicture->setTransformToFit(getLocalBounds().toFloat(), 0);
#else
        DrawableComposite *dc = dynamic_cast<DrawableComposite *>(piecePicture.get());
        if (dc != nullptr)
        {
            RectanglePlacement placement(0);
            dc->setTransform
                (placement.getTransformToFit(dc->getContentArea().resolve(nullptr),
                                             getLocalBounds().toFloat()));
        }
#endif
    }
}

void SquareComponent::SetPiece(Piece p)
{
    if (piece == p)
    {
        return; // no-op
    }

    piece = p;
    
    // May remove an old piece.
    if ((piecePicture = gPieceCache->GetNew(piece)) != nullptr)
    {
        transformPiece();
        // Need to draw the new piece we just created.
        addAndMakeVisible(piecePicture);
    }
}

void SquareComponent::resized()
{
    Component::resized();
    transformPiece();
}
