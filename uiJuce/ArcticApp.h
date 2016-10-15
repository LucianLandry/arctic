//--------------------------------------------------------------------------
//              ArcticApp.h - Juce application object for Arctic
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
