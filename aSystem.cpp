//--------------------------------------------------------------------------
//                  aSystem.cpp - system platform utilities.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>     // mkdir(2)
#include <sys/time.h>     // get+setrlimit(2)
#include <sys/resource.h>
#include <thread>         // hardware_concurrency()
#include <unistd.h>       // sysconf(3)

#include "aSystem.h"
#include "log.h"
#include "ref.h"

void SystemEnableCoreFile()
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
int64 SystemTotalMemory()
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
        result = INT64_MAX; // assume we overflowed.
    return result;
}

int SystemTotalProcessors()
{
    // sysconf(_SC_NPROCESSORS_ONLN) works; but this is more portable:
    return std::thread::hardware_concurrency();
}

std::string SystemAppDirectory()
{
    char *homePath = getenv("HOME");
    if (homePath == nullptr)
        return "";
    std::string result = homePath;
    result += "/.arctic";

    // As a convenience, create this directory if it does not already exist.
    return !mkdir(result.c_str(), 0700) || errno == EEXIST ? result : "";
}

std::string SystemNullFile()
{
    return "/dev/null"; // or "NUL", if we implement for Windows
}
