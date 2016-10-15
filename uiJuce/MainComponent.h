//--------------------------------------------------------------------------
//                 MainComponent.h - content of main window
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef MAINCOMPONENT_H
#define MAINCOMPONENT_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::Component
#include "BoardComponent.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent();
    void resized(); // override
private:
    BoardComponent bc;
};

#endif // MAINCOMPONENT_H
