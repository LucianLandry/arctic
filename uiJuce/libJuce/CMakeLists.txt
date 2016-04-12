# Builds the Juce library -- or more correctly, the parts we care about.
include_directories("/usr/include/freetype2") # needed by juce_graphics

add_library(juce ${JUCE_PATH}/modules/juce_core/juce_core.cpp ${JUCE_PATH}/modules/juce_data_structures/juce_data_structures.cpp ${JUCE_PATH}/modules/juce_events/juce_events.cpp ${JUCE_PATH}/modules/juce_graphics/juce_graphics.cpp ${JUCE_PATH}/modules/juce_gui_basics/juce_gui_basics.cpp)
