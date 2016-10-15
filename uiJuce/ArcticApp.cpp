//--------------------------------------------------------------------------
//              ArcticApp.cpp - Juce application object for Arctic
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <stdio.h> // printf()
#include <stdlib.h> // exit()

#include "ArcticApp.h"
#include "PieceCache.h"

using namespace juce;

ArcticApp::ArcticApp()
{
}

ArcticApp::~ArcticApp()
{
}

void ArcticApp::initialise(const String& commandLine)
{
    gPieceCache = new PieceCache();

    acm.registerAllCommandsForTarget(this);
    myMainWindow = new MainWindow();
    printf("bldbg: standalone %d\n", JUCEApplicationBase::isStandaloneApp());
    myMainWindow->addKeyListener(acm.getKeyMappings()); // ctrl+q -> quit
}

void ArcticApp::shutdown()
{
    myMainWindow = 0;
    delete gPieceCache;

    printf("bye.\n");
}

const String ArcticApp::getApplicationName()
{
    return "arctic";
}

const String ArcticApp::getApplicationVersion()
{
    return
        VERSION_STRING_MAJOR "."
        VERSION_STRING_MINOR "-"
        VERSION_STRING_PHASE;
}

void ArcticApp::getCommandInfo(CommandID commandID, ApplicationCommandInfo &result)
{
    printf("bldbg:ArcticApp::getCommandInfo(): invoked 0x%x\n", commandID);
    JUCEApplication::getCommandInfo(commandID, result);
}

void ArcticApp::getAllCommands(Array<CommandID> &commands)
{
    printf("bldbg:ArcticApp::getAllCommands(): invoked\n");
    commands.clear(); // just in case
    JUCEApplication::getAllCommands(commands);
}

bool ArcticApp::perform(const ApplicationCommandTarget::InvocationInfo &info)
{
    if (JUCEApplication::perform(info)) // this handles quit
    {
        return true; // parent handled
    }
    printf("bldbg: ArcticApp::perform invoked, command %d\n", info.commandID);
    return true;
}
