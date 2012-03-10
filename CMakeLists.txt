project (arctic)

add_executable(arctic aThread.c board.c clock.c comp.c conio.c game.c gDynamic.c gPreCalc.c list.c log.c main.c moveList.c playloop.c saveGame.c switcher.c thinker.c uiNcurses.c uiUci.c uiUtil.c uiXboard.c)

target_link_libraries(arctic ncurses pthread)

# Normal compile flags.
set(CMAKE_C_FLAGS "-Wall -Werror -O2 -fomit-frame-pointer")
# Fast debug flags.
# set(CMAKE_C_FLAGS -Wall -Werror -g -O2 -DENABLE_DEBUG_LOGGING)
# Debug flags.
# set(CMAKE_C_FLAGS -Wall -Werror -g -DENABLE_DEBUG_LOGGING)
# Profiling flags.  NOTE: keep the program up (say) 4 seconds after the game
# finishes, otherwise you may get inaccurate (too small) counts. (scratch that,
# it happens anyway, usually...)
# set(CMAKE_C_FLAGS -Wall -Werror -O2 -pg -fno-inline-functions-called-once)

# Normal link flags.
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_C_FLAGS})
