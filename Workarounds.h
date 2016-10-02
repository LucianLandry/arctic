//--------------------------------------------------------------------------
//           Workarounds.h - workarounds for broken libraries, etc.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
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

#ifndef WORKAROUNDS_H
#define WORKAROUNDS_H

// std::is_trivially_copyable was not provided prior to libstdc++5.0, so we
//  provide it here if necessary.
// Lifted from: https://github.com/mpark/variant/issues/8.
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20150801

// We would #include <type_traits> here, but our source files should have done
//  that already.
namespace std
{
template <typename T>
struct is_trivially_copyable : integral_constant<bool, __has_trivial_copy(T)> {
};
} // namespace std
#endif

#endif // WORKAROUNDS_H
