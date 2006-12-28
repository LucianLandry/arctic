#include <stdarg.h>
#include <stdio.h>
#include "ref.h"

static int gLogLevel = eLogEmerg;


void LogSetLevel(int level)
{
    gLogLevel = level;
}


int LogPrint(int level, const char *format, ...)
{
    va_list ap;
    int rv = 0;

    if (level <= gLogLevel)
    {
	va_start(ap, format);
	rv = vfprintf(stderr, format, ap);
	va_end(ap);
    }

    return rv;
}


/* debugging funcs. */
void LogMoveList(int level, struct mlist *mvlist)
{
    int i = 0;
    if (level > gLogLevel)
    {
	return; /* no-op. */
    }

    LogPrint(eLogDebug, "{mvlist lgh %d insrt %d ",
	     mvlist->lgh, mvlist->insrt);
    for (i = 0; i < mvlist->lgh; i++)
    {
	LogPrint(level,
 "%c%c%c%c ",
		 File(mvlist->list[i] [0]) + 'a',
		 Rank(mvlist->list[i] [0]) + '1',
		 File(mvlist->list[i] [1]) + 'a',
		 Rank(mvlist->list[i] [1]) + '1');
    }
    LogPrint(level, "}\n");
}

void LogMove(int level, int moveDepth, uint8 *comstr)
{
    if (level > gLogLevel)
    {
	return; /* no-op */
    }
    for (; moveDepth > 0; moveDepth--)
    {
	LogPrint(level, "    ");
    }
    LogPrint(level, "%c%c%c%c\n",
	     File(comstr[0]) + 'a',
	     Rank(comstr[0]) + '1',
	     File(comstr[1]) + 'a',
	     Rank(comstr[1]) + '1');
}
