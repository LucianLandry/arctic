if (JUCE_PATH)
    include_directories(${JUCE_PATH}/JuceLibraryCode/modules)
endif (JUCE_PATH)

# It is critical to get this correct.  Juce classes can change in allocated
# size and/or layout when they are built with debugging, so if the application
# is not built with the same flags, it will get very weird runtime errors.
if (JUCE_DEBUG)
    set(JUCE_DEBUG_FLAGS "-DDEBUG=1 -D_DEBUG=1") 
else (JUCE_DEBUG)
    set(JUCE_DEBUG_FLAGS "-DNDEBUG=1") 
endif (JUCE_DEBUG)

include_directories(${PROJECT_SOURCE_DIR})

# juce_Atomic.h (#included indirectly) trips up the compiler w/strict-
# aliasing warnings, although the code itself looks (basically) clean.
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-strict-aliasing ${JUCE_DEBUG_FLAGS}")

add_library(uiJuce ArcticApp.cpp BoardComponent.cpp MainComponent.cpp MainMenuBarModel.cpp MainWindow.cpp PieceCache.cpp SquareComponent.cpp uiJuce.cpp)
