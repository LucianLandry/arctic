//--------------------------------------------------------------------------
//                     MainWindow.h - main JUCE window
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
