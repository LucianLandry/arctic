//--------------------------------------------------------------------------
//              PieceComponent.h - UI representation of a piece.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef PIECE_COMPONENT_H
#define PIECE_COMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::Drawable

class PieceComponent : public juce::Drawable
{
public:
    PieceComponent(int type);
    ~PieceComponent();
    int getType();
private:
    int type;
};

#endif // PIECE_COMPONENT_H
