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

SquareComponent::SquareComponent()
{
    colour = Colours::black;
    _pieceType = 0;
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
    if (piece != nullptr)
    {
#if 0
	piece->setTransformToFit(getLocalBounds().toFloat(), 0);
#else
	DrawableComposite *dc = dynamic_cast<DrawableComposite *>(piece.get());
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

void SquareComponent::setPieceType(int pieceType)
{
    if (_pieceType == pieceType)
    {
	return; // no-op
    }

    _pieceType = pieceType;
    // May remove an old piece.
    if ((piece = gPieceCache->getNew(pieceType)) != nullptr)
    {
	transformPiece();
	// Need to draw the new piece we just created.
	addAndMakeVisible(piece);
    }
}

void SquareComponent::resized()
{
    Component::resized();
    transformPiece();
}
