#-----------------------------------------------------------------------------
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#-----------------------------------------------------------------------------

# Builds the Juce library -- or more correctly, the parts we care about.
include_directories("/usr/include/freetype2") # needed by juce_graphics

add_library(juce ${JUCE_PATH}/modules/juce_core/juce_core.cpp ${JUCE_PATH}/modules/juce_data_structures/juce_data_structures.cpp ${JUCE_PATH}/modules/juce_events/juce_events.cpp ${JUCE_PATH}/modules/juce_graphics/juce_graphics.cpp ${JUCE_PATH}/modules/juce_gui_basics/juce_gui_basics.cpp)
