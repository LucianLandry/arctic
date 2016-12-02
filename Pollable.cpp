//--------------------------------------------------------------------------
//               Pollable.cpp - Pollable object representation.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>     // assert(3)
#include <errno.h>      // errno(3)
#include <unistd.h>     // close(2)
#include "aTypes.h"     // uint64
#include "Pollable.h"

#ifdef HAS_EVENTFD
#include <sys/eventfd.h>
Pollable::Pollable()
{
    eventFd = eventfd(0, 0);
    assert(eventFd != -1);
}

Pollable::~Pollable()
{
    close(eventFd);
}

void Pollable::Ready()
{
    uint64 data = 1;
    int sent;
    
    while ((sent = write(eventFd, &data, 8)) < 0 && errno == EINTR)
        ;
    assert(sent == 8);
}

void Pollable::NotReady()
{
    uint64 data;
    int recvd;
    while ((recvd = read(eventFd, &data, 8)) < 0 && errno == EINTR)
        ;
    assert(recvd == 8);
}

int Pollable::Fd() const
{
    return eventFd;
}

#else // !HAS_EVENTFD
#include <sys/socket.h> // socketpair(2)

Pollable::Pollable()
{
    int socks[2];
    int err = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
    assert(err == 0);
    int bufSize = 1;
    for (int i = 0; i < 2; i++)
    {
        err = setsockopt(socks[i], SOL_SOCKET, SO_RCVBUF,
                         &bufSize, sizeof(bufSize));
        // asserting is optional, but we'd like to catch invalid args so we can
        //  fix them.
        assert(err == 0);
        err = setsockopt(socks[i], SOL_SOCKET, SO_SNDBUF,
                         &bufSize, sizeof(bufSize));
        assert(err == 0);
    }
    readSock = socks[0];
    writeSock = socks[1];
}

Pollable::~Pollable()
{
    close(readSock);
    close(writeSock);
}

void Pollable::Ready()
{
    int sent;
    char buf = 0;

    while ((sent = send(writeSock, &buf, 1, 0)) < 0 && errno == EINTR)
        ;
    assert(sent > 0);
}

void Pollable::NotReady()
{
    int recvd;
    char buf;
    while ((recvd = recv(readSock, &buf, 1, 0)) < 0 && errno == EINTR)
        ;
    assert(recvd > 0);
}

int Pollable::Fd() const
{
    return readSock;
}
#endif // HAS_EVENTFD
