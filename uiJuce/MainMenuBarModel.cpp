//--------------------------------------------------------------------------
//                 MainMenuBarModel.cpp - main menubar model
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
