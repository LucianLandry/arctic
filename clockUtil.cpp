//--------------------------------------------------------------------------
//                clockUtil.cpp - supplementary clock routines
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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
#include <sys/time.h> // gettimeofday(2)

#include "clockUtil.h"

#define CLOCK_TIME_INFINITE_STR "inf"

// We want either xx:yy:zz, yy:zz, or (:)zz.
// (or CLOCK_TIME_INFINITE_STR, --> "inf")
// But we try to be permissive in what we accept.
bool TimeStringIsValid(char *str)
{
    int colCount = 0;
    bool isValid = true;

    if (!strcmp(str, CLOCK_TIME_INFINITE_STR))
    {
        return true;
    }

    if (*str == '\0')
    {
        return false;
    }
    do
    {
        if (// must be digit or ':'
            (!isdigit(*str) && *str != ':') ||
            // ':' must have a number after it, and cannot be > 2 numbers.
            (*str == ':' && (++colCount > 2 || !isdigit(*(str + 1)))))
        {
            isValid = false;
            break;
        }
    } while (*(++str) != '\0');

    return isValid;
}


// Count number of occurances of 'needle' in 'haystack'.
static int strCount(const char *haystack, const char *needle)
{
    int retVal = 0;
    const char *occur;

    for (retVal = 0;
         (occur = strstr(haystack, needle)) != NULL;
         haystack = occur + strlen(needle), retVal++)
        ; // no-op

    return retVal;
}


// Returns numerical form of time.  Asserts if invalid time.  This is to make
// sure we catch coding errors -- user input should be checked with
// TimeStringIsValid().  (This does currently mean we cannot convert a negative
// time, even though TimeStringFromBigTime can go the other way with a negative
// time.)
bigtime_t TimeStringToBigTime(char *str)
{
    int hours = 0, minutes = 0, seconds = 0;

    if (!TimeStringIsValid(str))
        assert(0);

    if (!strcmp(str, CLOCK_TIME_INFINITE_STR))
        return CLOCK_TIME_INFINITE;

    switch(strCount(str, ":"))
    {
    case 2:
        if (sscanf(str, "%d:%d:%d", &hours, &minutes, &seconds) < 3 &&
            sscanf(str, ":%d:%d", &minutes, &seconds) < 2)
        {
            assert(0);
        }
        break;
    case 1:
        if (sscanf(str, "%d:%d", &minutes, &seconds) < 2 &&
            sscanf(str, ":%d", &seconds) < 1)
        {
            assert(0);
        }
        break;
    case 0:
        if (sscanf(str, "%d", &seconds) < 1)
        {
            assert(0);
        }
        break;
    default:
        assert(0);
    }

    return ((bigtime_t) (hours * 3600 + minutes * 60 + seconds)) * 1000000;
}


// stores string representation of 'myTime' into 'result'.
// Also returns 'result'.
// 'result' must be at least CLOCK_TIME_STR_LEN bytes long.
char *TimeStringFromBigTime(char *result, bigtime_t myTime)
{
    int hours, minutes, seconds;
    int negative;

    if (myTime == CLOCK_TIME_INFINITE)
    {
        sprintf(result, CLOCK_TIME_INFINITE_STR);
        return result;
    }

    // This rounds us up to the nearest second.
    // (The -1000000 check gets the math right for negative numbers.)
    if (myTime > -1000000 && myTime % 1000000)
    {
        myTime += 1000000;
    }

    negative = (myTime < 0);
    myTime = llabs(myTime);

    // Direction of truncation is machine-dependent for negative numbers,
    // so make sure we only use positive numbers here.
    myTime /= 1000000; // convert to seconds representation

    hours = myTime / 3600;
    myTime -= (hours * 3600);
    minutes = myTime / 60;
    myTime -= (minutes * 60);
    seconds = myTime;

    if (hours)
    {
        sprintf(result, "%s%d:%02d:%02d",
                negative ? "-" : "", hours, minutes, seconds);
    }
    else if (minutes)
    {
        sprintf(result, "%s%d:%02d",
                negative ? "-" : "", minutes, seconds);
    }
    else
    {
        sprintf(result, "%s:%02d",
                negative ? "-" : "", seconds);
    }
    return result;
}

bigtime_t CurrentTime(void)
{
    struct timeval tmvalue;
    gettimeofday(&tmvalue, NULL);
    return ((bigtime_t) tmvalue.tv_sec) * 1000000 + tmvalue.tv_usec;
}
