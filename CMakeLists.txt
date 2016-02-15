# Older versions might work, but I have not tested them.
cmake_minimum_required(VERSION 2.8.2)

project (arctic)

# Juce dependencies.
set (JUCE_DEP_LIBS rt dl GL X11 Xext freetype)

if (JUCE_PATH)
    # Point at a custom pre-built Juce build.  Most of the time you will want
    # this, because most systems do not come w/Juce by default.
    set (EXTRA_LIBS ${EXTRA_LIBS} "${JUCE_PATH}/Builds/LinuxMakefile/build/libjuce.a" ${JUCE_DEP_LIBS})
else (JUCE_PATH)
    # Assume the Juce library has been installed on the system and is available
    # as a dynamic library called "juce".
    set (EXTRA_LIBS ${EXTRA_LIBS} juce ${JUCE_DEP_LIBS})
endif (JUCE_PATH)

# Variables like 'CMAKE_C_FLAGS' should be set before adding subdirectories
# (unless you do not want them to be passed down).

# Standard compile flags.
# set(CMAKE_C_FLAGS "-Wall -Werror -O2 -fomit-frame-pointer")
# Fast debug flags.  In practice, this seems to be basically as fast as the
# "normal" flags, so we actually normally use this.
set(CMAKE_C_FLAGS "-Wall -Werror -g -O2 -fomit-frame-pointer")
# Debug flags.
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

add_executable(arctic aThread.cpp aSystem.cpp Board.cpp BoardMoveGen.cpp Clock.cpp clockUtil.cpp comp.cpp conio.c Eval.cpp game.cpp gDynamic.cpp gPreCalc.cpp list.cpp log.cpp main.cpp move.cpp MoveList.cpp Piece.cpp playloop.cpp Position.cpp pv.cpp saveGame.cpp switcher.cpp thinker.cpp transTable.cpp uiNcurses.cpp uiUci.cpp uiUtil.cpp uiXboard.cpp Variant.cpp)

# ncursesw is necessary for a UTF-8 console cursor.
target_link_libraries(arctic ncursesw pthread uiJuce ${EXTRA_LIBS})

# Drop -rdynamic since I am pretty sure we do not need it and it bloats the
# executable.
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
