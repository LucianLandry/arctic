//--------------------------------------------------------------------------
//
//                  aSystem.h - system platform utilities.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
