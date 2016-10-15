//--------------------------------------------------------------------------
//                 stringUtil.cpp - string support routines.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <ctype.h>
#include <string.h>

#include "stringUtil.h"

const char *findNextNonWhiteSpace(const char *str)
{
    if (str == NULL)
        return NULL;
    while (isspace(*str) && *str != '\0')
        str++;
    return *str != '\0' ? str : NULL;
}

const char *findNextWhiteSpace(const char *str)
{
    const char *result = findNextWhiteSpaceOrNull(str);
    return *result != '\0' ? result : NULL;
}

const char *findNextWhiteSpaceOrNull(const char *str)
{
    if (str == NULL)
        return NULL;
    while (!isspace(*str) && *str != '\0')
        str++;
    return str;
}

char *copyToken(char *dst, int dstLen, const char *src)
{
    int srcLen;

    if (src == NULL ||
        (srcLen = (findNextWhiteSpaceOrNull(src) - src)) >= dstLen)
    {
        return NULL;
    }
    memcpy(dst, src, srcLen);
    dst[srcLen] = '\0';
    return dst;
}

const char *findNextToken(const char *str)
{
    return
        str == NULL ? NULL :
        findNextNonWhiteSpace(isspace(*str) ? str :
                              findNextWhiteSpace(str));
}

static bool matchHelper(const char *str, const char *needle, bool caseSensitive)
{
    int len = strlen(needle);
    return
        str == NULL ? 0 :
        !(caseSensitive ? strncmp(str, needle, len) :
          strncasecmp(str, needle, len)) &&
        (isspace(str[len]) || str[len] == '\0');
}

bool matches(const char *str, const char *needle)
{
    return matchHelper(str, needle, true);
}

bool matchesNoCase(const char *str, const char *needle)
{
    return matchHelper(str, needle, false);
}

bool isNewLineChar(char c)
{
    return c == '\n' || c == '\r';
}

char *ChopBeforeNewLine(char *s)
{
    char *origStr = s;

    for (; *s != '\0'; s++)
    {
        if (isNewLineChar(*s))
        {
            *s = '\0';
            return origStr;
        }
    }
    return origStr;
}
