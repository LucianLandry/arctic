//--------------------------------------------------------------------------
//              BoardComponent.h - UI representation of a board.
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

#ifndef BOARD_COMPONENT_H
#define BOARD_COMPONENT_H

#include "../AppConfig.h"
#include "juce_gui_basics/juce_gui_basics.h" // juce::Component

#include "Position.h"
#include "ref.h" // NUM_SQUARES

#include "SquareComponent.h"

class BoardComponent : public juce::Component
{
public:
    BoardComponent();
    ~BoardComponent();
    void resized() override;

    // re-draw the component using the contents of 'position'.
    void refresh(const Position &position);
private:
    SquareComponent squares[NUM_SQUARES];
};

#endif // BOARD_COMPONENT_H
