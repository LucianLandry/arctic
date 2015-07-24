//--------------------------------------------------------------------------
//          SquareComponent.cpp - UI representation of a board square.
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
