//--------------------------------------------------------------------------
//                  stringUtil.h - string support routines.
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
