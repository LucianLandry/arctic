project (arctic)

add_executable(arctic aThread.c aSystem.c board.c clock.c comp.c conio.c game.c gDynamic.c gPreCalc.c list.c log.c main.c moveList.c playloop.c saveGame.c switcher.c thinker.c transTable.c uiNcurses.c uiUci.c uiUtil.c uiXboard.c)

target_link_libraries(arctic ncurses pthread)

# Normal compile flags.
# set(CMAKE_C_FLAGS "-Wall -Werror -O2 -fomit-frame-pointer")
# Fast debug flags.
set(CMAKE_C_FLAGS "-Wall -Werror -g -O2")
# Debug flags.
# set(CMAKE_C_FLAGS "-Wall -Werror -g -DENABLE_DEBUG_LOGGING")
# Profiling flags.  NOTE: keep the program up (say) 4 seconds after the game
# finishes, otherwise you may get inaccurate (too small) counts. (scratch that,
# it happens anyway, usually...)
# set(CMAKE_C_FLAGS "-Wall -Werror -O2 -pg -fno-inline-functions-called-once")

# Drop -rdynamic since I am pretty sure we do not need it
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
