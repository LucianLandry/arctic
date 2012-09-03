//--------------------------------------------------------------------------
//                     MainWindow.cpp - main JUCE window
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

#include <stdio.h>

#include "ref.h" // MIN()
#include "MainWindow.h"

using namespace juce;

MainWindow::MainWindow()
    : DocumentWindow (JUCEApplication::getInstance()->getApplicationName(),
                      Colours::lightgrey,
                      DocumentWindow::allButtons)
{
    centreWithSize (500, 400);
    setContentNonOwned(&mc, false);
    setResizable(true, false);
    setMenuBar(&mmbm);
    setVisible(true);
}

MainWindow::~MainWindow()
{
    printf("bldbg:MainWindow::~MainWindow()\n");
    setMenuBar(0);
}

void MainWindow::closeButtonPressed()
{
    JUCEApplication::getInstance()->systemRequestedQuit();
}
