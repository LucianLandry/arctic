#-----------------------------------------------------------------------------
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#-----------------------------------------------------------------------------

# cmake versions older than 3.0.2 are not tested.  RHEL7 (2.8.11) brings up the
# rear here.  Qt also recommends >= 2.8.11 (but only requires 2.8.3); see
# http://doc.qt.io/qt-5/cmake-manual.html.
cmake_minimum_required(VERSION 2.8.11)
project (arctic)

# Variables like 'CMAKE_C_FLAGS' should be set before adding subdirectories
# (unless you do not want them to be passed down).

# Standard compile flags.
# set(CMAKE_C_FLAGS "-Wall -O2 -fomit-frame-pointer")
# Fast debug flags.  In practice, this seems to be basically as fast as the
# "normal" flags, so we actually normally use this.
set(CMAKE_C_FLAGS "-Wall -g -O2 -fomit-frame-pointer")
# Debug flags.  This can slow things down *considerably* (~6x)!
# set(CMAKE_C_FLAGS "-Wall -g -DENABLE_DEBUG_LOGGING -fno-omit-frame-pointer")
# Profiling flags.  NOTE: keep the program up (say) 4 seconds after the game
# finishes, otherwise you may get inaccurate (too small) counts. (scratch that,
# it happens anyway, usually...)
# I would use -fno-inline-functions-called-once here (instead of -fno-inline),
# but clang does not implement it.
# set(CMAKE_C_FLAGS "-Wall -O2 -pg -fno-inline -fno-omit-frame-pointer")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")

# The ONLY reason this is off by default is to aid potential packagers and
# end-users.  As a developer, I prefer to run with this enabled.
option(ENABLE_STRICT_COMPILE "Be strict about sloppy code" OFF)
if(ENABLE_STRICT_COMPILE STREQUAL "ON")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif(ENABLE_STRICT_COMPILE STREQUAL "ON")

add_executable(arctic aList.cpp aSemaphore.cpp aSystem.cpp Board.cpp BoardMoveGen.cpp Clock.cpp clockUtil.cpp comp.cpp Config.cpp conio.c Engine.cpp Eval.cpp EventQueue.cpp Game.cpp gPreCalc.cpp HistoryWindow.cpp log.cpp main.cpp move.cpp MoveList.cpp Piece.cpp playloop.cpp Pollable.cpp Position.cpp Pv.cpp SaveGame.cpp stringUtil.cpp Switcher.cpp Thinker.cpp Timer.cpp TransTable.cpp uiNcurses.cpp uiUci.cpp uiUtil.cpp uiXboard.cpp Variant.cpp)

# Juce dependencies.
option(ENABLE_UI_JUCE "Enable a Juce-based GUI (experimental)" OFF)
if(ENABLE_UI_JUCE STREQUAL "ON")
    # If you choose to build with Juce, you must also define JUCE_PATH to the
    #  root of the Git snapshot you downloaded.  Minimal example:
    # mkdir ~/juce; cd ~/juce;
    # git clone --depth 1 git://github.com/julianstorer/JUCE.git
    # cd -; cmake -DENABLE_UI_JUCE="ON" -DJUCE_PATH=${HOME}/juce/JUCE (...)
    option(ENABLE_UI_JUCE_DEBUG "Build juce+uiJuce with debugging enabled" ON)
    add_subdirectory(uiJuce)
    set(JUCE_DEP_LIBS rt dl GL X11 Xext freetype)
    set(EXTRA_LIBS ${EXTRA_LIBS} uiJuce juce ${JUCE_DEP_LIBS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_UI_JUCE=1")
endif(ENABLE_UI_JUCE STREQUAL "ON")

# Qt dependencies.
option(ENABLE_UI_QT "Enable a Qt-based GUI (experimental)" OFF)
if(ENABLE_UI_QT STREQUAL "ON")
    add_subdirectory(uiQt)    
    find_package(Qt5 COMPONENTS Widgets Svg REQUIRED)
    set(EXTRA_LIBS ${EXTRA_LIBS} uiQt Qt5::Widgets Qt5::Svg)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_UI_QT=1")
endif(ENABLE_UI_QT STREQUAL "ON")

# ncursesw (instead of ncurses) is necessary for a UTF-8 console cursor.
target_link_libraries(arctic ncurses pthread ${EXTRA_LIBS})

# Drop -rdynamic since I am pretty sure we do not need it and it bloats the
# executable.
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
