//--------------------------------------------------------------------------
//                 aTypes.h - basic type definitions for Arctic.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef ATYPES_H
#define ATYPES_H

#include <stdbool.h> // C99 "bool".
#include <stdint.h>  // C99 integer types, INT64_MAX, SIZE_MAX, etc.
#include <inttypes.h> // C99 support for platform-agnostic printf() of 64-bit
                      // types (PRId64 for example)

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int uint;

typedef uint8_t      uint8;
typedef int8_t       int8;
typedef uint16_t     uint16;
typedef uint32_t     uint32;
typedef uint64_t     uint64;
typedef int64_t      int64;

// Time in microseconds.  Inspired by BeOS (only not; we need to support
// negative times, too.)
typedef int64 bigtime_t;

#ifdef __cplusplus
}
#endif

#endif // ATYPES_H
