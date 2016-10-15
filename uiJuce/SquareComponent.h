//--------------------------------------------------------------------------
//           SquareComponent.h - UI representation of a board square.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
