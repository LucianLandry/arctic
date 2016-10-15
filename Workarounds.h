//--------------------------------------------------------------------------
//           Workarounds.h - workarounds for broken libraries, etc.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
