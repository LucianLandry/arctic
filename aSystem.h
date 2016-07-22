//--------------------------------------------------------------------------
//
//                  aSystem.h - system platform utilities.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef ASYSTEM_H
#define ASYSTEM_H

#include "aTypes.h" // int64 etc.

#ifdef __cplusplus
extern "C" {
#endif

void SystemEnableCoreFile(void);
int64 SystemTotalMemory(void);
int SystemTotalProcessors(void);

#ifdef __cplusplus
}
#endif

#endif // ASYSTEM_H
