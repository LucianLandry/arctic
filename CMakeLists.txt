# Older versions might work, but I have not tested them.
cmake_minimum_required(VERSION 3.0.2)
project (arctic)

# Variables like 'CMAKE_C_FLAGS' should be set before adding subdirectories
# (unless you do not want them to be passed down).

# Standard compile flags.
# set(CMAKE_C_FLAGS "-Wall -Werror -O2 -fomit-frame-pointer")
# Fast debug flags.  In practice, this seems to be basically as fast as the
# "normal" flags, so we actually normally use this.
set(CMAKE_C_FLAGS "-Wall -Werror -g -O2 -fomit-frame-pointer")
# Debug flags.  This can slow things down *considerably* (~6x)!
# set(CMAKE_C_FLAGS "-Wall -Werror -g -DENABLE_DEBUG_LOGGING -fno-omit-frame-pointer")
# Profiling flags.  NOTE: keep the program up (say) 4 seconds after the game
# finishes, otherwise you may get inaccurate (too small) counts. (scratch that,
# it happens anyway, usually...)
# Also I would use -fno-inline-functions-called-once here (instead of
# -fno-inline), but that option has apparently been removed in later versions
# of GCC.
# set(CMAKE_C_FLAGS "-Wall -Werror -O2 -pg -fno-inline -fno-omit-frame-pointer")

# Propagate anything we need (like -pg) to the c++ linker.
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")

# Juce dependencies.
option(ENABLE_UI_JUCE "Enable a Juce-based GUI (experimental)" OFF)
if(ENABLE_UI_JUCE STREQUAL "ON")
    # If you choose to build with Juce, you must also define JUCE_PATH to the
    #  root of the Git snapshot you downloaded.  Minimal example:
    # mkdir ~/juce; cd ~/juce;
    # git clone --depth 1 git://github.com/julianstorer/JUCE.git
    # cd -; cmake -DENABLE_UI_JUCE="ON" -DJUCE_PATH=~/juce/JUCE (...)
    option(ENABLE_UI_JUCE_DEBUG "Build juce+uiJuce with debugging enabled" ON)
    add_subdirectory(uiJuce)
    set(JUCE_DEP_LIBS rt dl GL X11 Xext freetype)
    set(EXTRA_LIBS uiJuce juce ${JUCE_DEP_LIBS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_UI_JUCE=1")
endif(ENABLE_UI_JUCE STREQUAL "ON")

add_executable(arctic aSemaphore.cpp aSystem.cpp Board.cpp BoardMoveGen.cpp Clock.cpp clockUtil.cpp comp.cpp Config.cpp conio.c Engine.cpp Eval.cpp Game.cpp gPreCalc.cpp HistoryWindow.cpp list.cpp log.cpp main.cpp move.cpp MoveList.cpp Piece.cpp playloop.cpp Position.cpp Pv.cpp SaveGame.cpp stringUtil.cpp Switcher.cpp Thinker.cpp TransTable.cpp uiNcurses.cpp uiUci.cpp uiUtil.cpp uiXboard.cpp Variant.cpp)

# ncursesw is necessary for a UTF-8 console cursor.
target_link_libraries(arctic ncursesw pthread ${EXTRA_LIBS})

# Drop -rdynamic since I am pretty sure we do not need it and it bloats the
# executable.
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
