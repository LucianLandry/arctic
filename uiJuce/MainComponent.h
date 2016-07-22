//--------------------------------------------------------------------------
//                 MainComponent.h - content of main window
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
