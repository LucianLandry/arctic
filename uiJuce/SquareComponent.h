//--------------------------------------------------------------------------
//           SquareComponent.h - UI representation of a board square.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
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

#ifndef SQUARE_COMPONENT_H
#define SQUARE_COMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::Component

class SquareComponent : public juce::Component
{
public:
    SquareComponent();
    ~SquareComponent();
    void paint(juce::Graphics &g) override;
    void colourChanged() override;
    void resized() override;

    void SetPiece(Piece p);
private:
    juce::Colour backgroundColour;

    juce::ScopedPointer<juce::Drawable> piecePicture;
    Piece piece;

    void transformPiece();
};

#endif // SQUARE_COMPONENT_H
