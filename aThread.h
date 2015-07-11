//--------------------------------------------------------------------------
//                       aThread.h - thread wrapper.
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

#ifndef ATHREAD_H
#define ATHREAD_H

#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

// Base class.  Meant to be opaque.
typedef struct {
    sem_t *mySem; // used for sync.
} ThreadArgsT;

// Useful for declaring instances of ThreadArgsT-derived classes.
extern ThreadArgsT gThreadDummyArgs;

typedef void *(*THREAD_FUNC)(void *);

// Spawn a child thread.
// 'args' must inherit from ThreadArgsT.
// Waits until child has called ThreadNotifyCreated() to continue execution.
//
// The return value is ignored.
void ThreadCreate(THREAD_FUNC childFunc, ThreadArgsT *args);

// Executed by the child thread.
// Notify the parent thread it is safe to continue execution (all arguments
// of interest have been copied off etc.)
// 'args' must be the same arguments passed to 'childFunc' above.
void ThreadNotifyCreated(const char *name, ThreadArgsT *args);

#ifdef __cplusplus
}
#endif

#endif // ATHREAD_H
