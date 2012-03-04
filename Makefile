# Simple makefile for (normally) building.

# (The _XOPEN_SOURCE define gets us the spinlock functionality.)
CPPFLAGS = -D_REENTRANT -D_XOPEN_SOURCE=600

# Normal flags.  It would be nice to use -flto, but Debian Squeeze's gcc
# does not support it.
CFLAGS = -Wall -Werror -O2 -fomit-frame-pointer
# Fast debug flags.
#CFLAGS = -Wall -Werror -g -O2 -DENABLE_DEBUG_LOGGING
# Debug flags.
#CFLAGS = -Wall -Werror -g -DENABLE_DEBUG_LOGGING
# Profiling flags.  NOTE: keep the program up (say) 4 seconds after the game
# finishes, otherwise you may get inaccurate (too small) counts. (scratch that,
# it happens anyway, usually...)
# CFLAGS = -Wall -Werror -O2 -pg -fno-inline-functions-called-once

LDFLAGS = $(CFLAGS)
LDLIBS = -lncurses -lpthread
OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

arctic : $(OBJECTS)
	$(CC) -o arctic $(LDFLAGS) $(OBJECTS) $(LDLIBS)

$(OBJECTS) : Makefile

clean :
	rm -f *.o arctic depend

depend: $(wildcard *.c) $(wildcard *.h)
	$(CC) -MM $(CPPFLAGS) *.c > depend

# Intended to force 'make' to start with a clean slate.
# Not sure if it does, though.
Makefile: depend

include depend
