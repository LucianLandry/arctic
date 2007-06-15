# Simple makefile for (normally) building.
# make myargs="-pg" for profiling.

# Normal flags.
CFLAGS = -Wall -Werror $(myargs) -O2 -fomit-frame-pointer -D_REENTRANT
# Debug flags.
#CFLAGS = -Wall -Werror $(myargs) -g -D_REENTRANT

LDFLAGS = $(CFLAGS)
LDLIBS = -lncurses -lpthread
OBJECTS = clock.o comp.o conio.o debug.o init.o intfXboard.o list.o log.o \
	main.o makemov.o movgen.o playmov.o saverestore.o switcher.o \
	thinker.o ui.o

arctic : $(OBJECTS)
	$(CC) -o arctic $(LDFLAGS) $(OBJECTS) $(LDLIBS)

$(OBJECTS) : ref.h list.h

clean :
	rm -f *.o arctic
