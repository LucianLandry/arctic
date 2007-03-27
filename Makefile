# Simple makefile for (normally) building.
# make myargs="-pg" for profiling.

CFLAGS = -Wall $(myargs) -O2 -D_REENTRANT
LDFLAGS = $(CFLAGS)
LDLIBS = -lncurses -lpthread
OBJECTS = comp.o conio.o debug.o init.o log.o main.o makemov.o movgen.o \
	playmov.o saverestore.o ui.o thinker.o switcher.o intfXboard.o

arctic : $(OBJECTS)
	$(CC) -o arctic $(LDFLAGS) $(OBJECTS) $(LDLIBS)

$(OBJECTS) : ref.h

clean :
	rm -f *.o arctic
