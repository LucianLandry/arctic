//--------------------------------------------------------------------------
//                  MainMenuBarModel.h - main menubar model
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
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
