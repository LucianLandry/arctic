//--------------------------------------------------------------------------
//              BoardComponent.h - UI representation of a board.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef BOARD_COMPONENT_H
#define BOARD_COMPONENT_H

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
