//--------------------------------------------------------------------------
//                 MainComponent.cpp - content of main window
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "MainComponent.h"

using namespace juce;

MainComponent::MainComponent()
{
    addAndMakeVisible(&bc);
}

MainComponent::~MainComponent()
{
}

static int normalize(int i)
{
    return (i / 8) * 8;
}

void MainComponent::resized()
{
    int bcYSize = getHeight();
    int bcXSize = getWidth();
    int bcSize = MIN(normalize(bcYSize), normalize(bcXSize));

    bc.setBounds(0,
                 getHeight() - bcSize,
                 bcSize, bcSize);
}

#if 0
void MainWindow::resized()
{
    DocumentWindow::resized();
    int bcYSize = getHeight() - getTitleBarHeight() - borderSize * 2;
    int bcXSize = getWidth() - borderSize * 2;
    int bcSize = MIN(normalize(bcYSize), normalize(bcXSize));

    bc.setBounds(borderSize,
                 getHeight() - bcSize - borderSize,
                 bcSize, bcSize);
}
#endif
