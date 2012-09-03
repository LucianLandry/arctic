//--------------------------------------------------------------------------
//                 MainComponent.cpp - content of main window
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
