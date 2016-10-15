//--------------------------------------------------------------------------
//                  MainMenuBarModel.h - main menubar model
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "juce_gui_basics/juce_gui_basics.h" // juce::MainMenuBarModel

#ifndef MAINMENUBARMODEL_H
#define MAINMENUBARMODEL_H

class MainMenuBarModel : public juce::MenuBarModel
{
public:
    MainMenuBarModel();
    ~MainMenuBarModel();

    // overridden from parent.  These are 'const' in Juce 2.0.
    juce::StringArray getMenuBarNames();
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                          const juce::String &menuName);
    void menuItemSelected(int menuItemID, int topLevelMenuIndex); 
private:
    juce::PopupMenu gameMenu;
};

#endif // MAINMENUBARMODEL_H
