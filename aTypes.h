//--------------------------------------------------------------------------
//                 aTypes.h - basic type definitions for Arctic.
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
