# It is critical to get this correct.  Juce classes can change in allocated
# size and/or layout when they are built with debugging, so if the application
# is not built with the same flags as the "library", it will get very weird
# runtime errors.
if (JUCE_DEBUG)
    set(JUCE_DEBUG_FLAGS "-DDEBUG=1 -D_DEBUG=1") 
else (JUCE_DEBUG)
    set(JUCE_DEBUG_FLAGS "-DNDEBUG=1") 
endif (JUCE_DEBUG)

# This is the stuff that would normally go into a Projucer-generated
# AppConfig.h.  Since we now build Juce's source directly, it is easier to just
# define everything here.  Alternatively, we could "-include AppConfig.h" or
# something similar.
set(JUCE_FLAGS "-DJUCE_MODULE_AVAILABLE_juce_core=1 -DJUCE_MODULE_AVAILABLE_juce_data_structures=1 -DJUCE_MODULE_AVAILABLE_juce_events=1 -DJUCE_MODULE_AVAILABLE_juce_graphics=1 -DJUCE_MODULE_AVAILABLE_juce_gui_basics=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1")

# Platform-specific junk.
set(JUCE_PLATFORM_SPECIFIC_FLAGS "-DLINUX=1")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${JUCE_DEBUG_FLAGS} ${JUCE_FLAGS} ${JUCE_PLATFORM_SPECIFIC_FLAGS}")

include_directories("${JUCE_PATH}/modules" "${PROJECT_SOURCE_DIR}/uiJuce/libJuce")

add_subdirectory(libJuce)

# libJuce shouldn't need to see anything in the normal source directory, so we
# include it separately.
include_directories("${PROJECT_SOURCE_DIR}")

add_library(uiJuce ArcticApp.cpp BoardComponent.cpp MainComponent.cpp MainMenuBarModel.cpp MainWindow.cpp PieceCache.cpp SquareComponent.cpp uiJuce.cpp)
