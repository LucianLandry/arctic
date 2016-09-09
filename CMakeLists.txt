# Older versions might work, but I have not tested them.
cmake_minimum_required(VERSION 2.8.2)

project (arctic)

# Juce dependencies.
set (JUCE_DEP_LIBS rt dl GL X11 Xext freetype)

set (EXTRA_LIBS ${JUCE_DEP_LIBS})

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

add_subdirectory(uiJuce)

add_executable(arctic aSemaphore.cpp aSystem.cpp Board.cpp BoardMoveGen.cpp Clock.cpp clockUtil.cpp comp.cpp Config.cpp conio.c Eval.cpp Game.cpp gPreCalc.cpp HistoryWindow.cpp list.cpp log.cpp main.cpp move.cpp MoveList.cpp Piece.cpp playloop.cpp Position.cpp Pv.cpp SaveGame.cpp switcher.cpp Thinker.cpp TransTable.cpp uiNcurses.cpp uiUci.cpp uiUtil.cpp uiXboard.cpp Variant.cpp)

# ncursesw is necessary for a UTF-8 console cursor.
target_link_libraries(arctic ncursesw pthread uiJuce juce ${EXTRA_LIBS})

# Drop -rdynamic since I am pretty sure we do not need it and it bloats the
# executable.
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
