//--------------------------------------------------------------------------
//                     MainWindow.cpp - main JUCE window
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
