//--------------------------------------------------------------------------
//              ArcticApp.h - Juce application object for Arctic
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

#ifndef ARCTIC_APP_H
#define ARCTIC_APP_H

#include "juce_gui_basics/juce_gui_basics.h" // JUCEApplication
#include "MainWindow.h"
#include "MainMenuBarModel.h"

class ArcticApp : public juce::JUCEApplication
{
    friend MainMenuBarModel::MainMenuBarModel();
public:
    ArcticApp();
    ~ArcticApp();

    void initialise(const juce::String& commandLine);
    void shutdown();
    const juce::String getApplicationName();
    const juce::String getApplicationVersion();

    // overridden from parent
    static ArcticApp *getInstance() noexcept
    {
        return dynamic_cast<ArcticApp *> (juce::JUCEApplication::getInstance());
    }

    // overridden from ApplicationCommandTarget:
    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo &result);
    void getAllCommands(juce::Array<juce::CommandID> &commands);
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo &info);

private:
    juce::ScopedPointer<MainWindow> myMainWindow;

    // Handles commands for app; see ApplicationCommandManager docs for details.
    juce::ApplicationCommandManager acm;
};

// juce::StandardApplicationCommandIDs::quit is the standard app quit command.
#if 0
typedef enum {
    eCmdQuit
} ArcticCmdT;
#endif // 0

#endif // ARCTIC_APP_H
