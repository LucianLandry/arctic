//--------------------------------------------------------------------------
//           SquareComponent.h - UI representation of a board square.
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

#ifndef SQUARE_COMPONENT_H
#define SQUARE_COMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::Component

class SquareComponent : public juce::Component
{
public:
    SquareComponent();
    ~SquareComponent();
    void paint(juce::Graphics &g); // override
    void colourChanged(); // override
    void resized(); // overridden from Component

    void setPieceType(int pieceType);
private:
    juce::Colour colour;

    juce::ScopedPointer<juce::Drawable> piece;
    int _pieceType;

    void transformPiece();
};

#endif // SQUARE_COMPONENT_H
