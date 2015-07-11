//--------------------------------------------------------------------------
//                       aThread.c - thread wrapper.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <pthread.h>
#include <assert.h>
#include "aThread.h"
#include "log.h"

ThreadArgsT gThreadDummyArgs = { NULL };

typedef void *(*PTHREAD_FUNC)(void *);

void ThreadCreate(THREAD_FUNC childFunc, ThreadArgsT *args)
{
    pthread_t myThread;
    sem_t mySem;
    int err;

    args->mySem = &mySem;

    err = sem_init(&mySem, 0, 0);
    assert (err == 0);
    err = pthread_create(&myThread, NULL, (PTHREAD_FUNC) childFunc, args);
    assert(err == 0);
    err = sem_wait(&mySem);
    assert(err == 0);
    err = sem_destroy(&mySem);
    assert(err == 0);
}

void ThreadNotifyCreated(const char *name, ThreadArgsT *args)
{
    sem_post(args->mySem);
    LOG_DEBUG("created thread \"%s\" %p\n", name, (void *) pthread_self());
}
