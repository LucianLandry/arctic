//--------------------------------------------------------------------------
//              PieceComponent.h - UI representation of a piece.
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
