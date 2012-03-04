//--------------------------------------------------------------------------
//
//                     aSpinlock.h - spinlock abstraction.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

// This is meant to be a platform-agnostic simple spinlock interface.
// Other platforms could be added here as required.  We could also
// implement fallbacks on platforms that lack spinlock primitives.

#ifndef ASPINLOCK_H
#define ASPINLOCK_H

#include <pthread.h>

#if 1 // True spinlock implementation.
typedef pthread_spinlock_t SpinlockT;
static inline int SpinlockInit(SpinlockT *lock)
{
    return pthread_spin_init(lock, 0);
}
static inline void SpinlockLock(SpinlockT *lock)
{
    pthread_spin_lock(lock);
}
static inline void SpinlockUnlock(SpinlockT *lock)
{
    pthread_spin_unlock(lock);
}

#else // Mutex implementation.  For comparative/testing purposes only.
typedef pthread_mutex_t SpinlockT;
static inline int SpinlockInit(SpinlockT *lock)
{
    return pthread_mutex_init(lock, NULL);
}
static inline void SpinlockLock(SpinlockT *lock)
{
    pthread_mutex_lock(lock);
}
static inline void SpinlockUnlock(SpinlockT *lock)
{
    pthread_mutex_unlock(lock);
}
#endif

#endif // ASPINLOCK_H
