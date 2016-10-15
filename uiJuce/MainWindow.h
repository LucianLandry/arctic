//--------------------------------------------------------------------------
//                     MainWindow.h - main JUCE window
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::DocumentWindow
#include "MainComponent.h"
#include "MainMenuBarModel.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow();
    ~MainWindow();
    // must override since juce:DocumentWindow::closeButtonPressed() is a noop
    void closeButtonPressed();
    // void resized(); // override
private:
    MainMenuBarModel mmbm;
    MainComponent mc;
};

#endif // MAINWINDOW_H
