//--------------------------------------------------------------------------
//                 MainMenuBarModel.cpp - main menubar model
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

#include "ArcticApp.h"
#include "MainMenuBarModel.h"

using namespace juce;

MainMenuBarModel::MainMenuBarModel()
{
    ApplicationCommandManager *acm = &ArcticApp::getInstance()->acm;
    gameMenu.addCommandItem(acm, StandardApplicationCommandIDs::quit, "Quit");
}

MainMenuBarModel::~MainMenuBarModel()
{
    printf("bldbg:MainMenuBarModel::~MainMenuBarModel()\n");
}

StringArray MainMenuBarModel::getMenuBarNames()
{
    char names[] = {"Game"};
    return StringArray(names);
}

PopupMenu MainMenuBarModel::getMenuForIndex(int topLevelMenuIndex,
                                            const String &menuName)
{
    return gameMenu;
}

void MainMenuBarModel::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    printf("bldbg: MainMenuBarModel::menuItemSelected(%d, %d) invoked\n",
	   menuItemID, topLevelMenuIndex);
}
