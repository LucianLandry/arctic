//--------------------------------------------------------------------------
//               playloop.cpp - main loop and support routines.
//                           -------------------
//  copyright            : (C) 2008 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <assert.h>
#include <errno.h>
#include <stdio.h>    // fileno(3)
#include <stdlib.h>   // abs(3)
#include <sys/poll.h> // poll(2)

#include "clockUtil.h"
#include "log.h"
#include "playloop.h"
#include "ui.h"

// Main play loop.
void PlayloopRun(Game &game, Thinker &th, Switcher &sw)
{
    struct pollfd pfds[2];
    int res;

    int tickTimeout, moveNowTimeout, pollTimeout;
    bool moveNowOnTimeout;
    bigtime_t myTime;
    int turn;

    // Setup the pollfd array.
    pfds[0].fd = fileno(stdin);
    pfds[0].events = POLLIN;
    pfds[1].fd = th.MasterSock();
    pfds[1].events = POLLIN;

    while (1)
    {
        tickTimeout = -1;
        moveNowTimeout = -1;
        pollTimeout = -1;
        turn = game.Board().Turn();
        moveNowOnTimeout = false;
        const Clock &myClock = game.Clock(turn);
        myTime = myClock.PerMoveTime();

        // When IsFirstMoveFree(), the clock will run on the first move even
        //  though we would rather it not.  Making it run makes the time recalc
        //  in the poll loop much more robust (since many things might happen
        //  to the clock in the meanwhile).
        // But, it means we should skip any tick notification.
        if (!myClock.IsFirstMoveFree() &&
            myClock.IsRunning() &&
            myTime != CLOCK_TIME_INFINITE)
        {
            // Try to keep the UI time display refreshed.

            // Start by finding usec until next tick.
            tickTimeout =
                (myTime < 0 ?
                 // Avoid machine-dependent behavior of / and % w/negative
                 // numbers.
                 1000000 - (llabs(myTime) % 1000000) :
                 myTime % 1000000); // normal case (positive)

            tickTimeout /= 1000; // ... converted to msec
            tickTimeout += 1; // ... and adjusted for division truncation
        }

        bigtime_t goalTime = th.GoalTime();
        // The computer cannot usually decide for itself when to move, so
        // we decide for it.
        if (goalTime != CLOCK_TIME_INFINITE)
        {
            moveNowTimeout = (myTime - goalTime) / 1000;
            moveNowTimeout = MAX(moveNowTimeout, 0);        

            moveNowOnTimeout = tickTimeout == -1 || moveNowTimeout < tickTimeout;
        }

        pollTimeout =
            tickTimeout != -1 && moveNowTimeout != -1 ?
            MIN(tickTimeout, moveNowTimeout) :
            tickTimeout != -1 ? tickTimeout :
            moveNowTimeout;

        // poll for input from either stdin (UI), or the computer, or timeout.
        res = poll(pfds, 2, pollTimeout);

        if (res == 0)
        {
            // poll timed out.  Do appropriate action and re-poll.
            if (moveNowOnTimeout)
                th.CmdMoveNow();
            else
                gUI->notifyTick(&game); // Tick, tock...
            continue;
        }

        if (res == -1 && errno == EINTR)
            continue;
        assert(res > 0); // other errors should not happen.
        assert(!(pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)));

        if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            LOG_DEBUG("stdin recvd event 0x%x, bailing\n", pfds[0].revents);
            return;
        }

        if (pfds[0].revents & POLLIN)
        {
            sw.Switch();
        }
        // 'else' because user-input handler may change the state on us,
        // so we need to re-poll...
        else if (pfds[1].revents & POLLIN)
        {
            th.ProcessOneRsp();
        }
    }
}
