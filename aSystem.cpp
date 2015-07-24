//--------------------------------------------------------------------------
//
//                  aSystem.c - system platform utilities.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
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

#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>     // get+setrlimit(2)
#include <sys/resource.h>
#include <unistd.h>       // sysconf(3)

#include "aSystem.h"
#include "gDynamic.h" // MAX_NUM_PROCS
#include "log.h"
#include "ref.h"


void SystemEnableCoreFile(void)
{
    struct rlimit rlimit;
    int rv;

    rv = getrlimit(RLIMIT_CORE, &rlimit);
    assert(rv == 0);

    if (rlimit.rlim_cur < rlimit.rlim_max)
    {
        rlimit.rlim_cur = rlimit.rlim_max;

        rv = setrlimit(RLIMIT_CORE, &rlimit);
        assert(rv == 0);
    }
}

// Caps total memory at INT64_MAX.
// Use GlobalMemoryStatusEx() on Windows?
int64 SystemTotalMemory(void)
{
    int64 result;
    long physPages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGESIZE);

    if (physPages <= 0 || pageSize <= 0)
    {
        LOG_EMERG("should not happen: physPages %ld pageSize %ld\n",
                  physPages, pageSize);
        exit(0);
    }
    result = (int64) physPages * (int64) pageSize;
    if (result < physPages || result < pageSize)
    {
        result = INT64_MAX; // assume we overflowed.
    }
    return result;
}

int SystemTotalProcessors(void)
{
    int result = sysconf(_SC_NPROCESSORS_ONLN);
    return MIN(result, MAX_NUM_PROCS);
}
