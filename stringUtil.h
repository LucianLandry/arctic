//--------------------------------------------------------------------------
//                  stringUtil.h - string support routines.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

// Token support routines.
const char *findNextNonWhiteSpace(const char *str);
const char *findNextWhiteSpace(const char *str);
const char *findNextWhiteSpaceOrNull(const char *str);

// Copies (possibly non-NULL-terminated) token 'src' to NULL-terminated 'dst'.
// Returns 'dst' iff a full copy could be performed, otherwise NULL (and in that
//  case, does not clobber 'dst', just to be nice)
// This is useful for isolating a token from the rest of the string.
char *copyToken(char *dst, int dstLen, const char *src);

// Given that we are pointing at a token 'str',
//  return a pointer to the token after it (or NULL, if none).
const char *findNextToken(const char *str);

// Pattern matchers for tokens embedded at the start of a larger string.
bool matches(const char *str, const char *needle);
bool matchesNoCase(const char *str, const char *needle);

// Returns true iff 'c' is a carriage return or linefeed.
bool isNewLineChar(char c);

// Terminate a string 's' at the 1st occurence of newline.  Returns 's' as a
// convenience.
char *ChopBeforeNewLine(char *s);

#endif // STRINGUTIL_H
