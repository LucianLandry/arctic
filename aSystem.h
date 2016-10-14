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

#include <string>
#include "aTypes.h" // int64 etc.

void SystemEnableCoreFile();
int64 SystemTotalMemory();
int SystemTotalProcessors();

// Return directory we should use to write logs and (in future?) other config.
std::string SystemAppDirectory();
// Return this system's equivalent of /dev/null.
std::string SystemNullFile();

#endif // ASYSTEM_H
