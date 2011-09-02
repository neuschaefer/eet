all: eet

CC=gcc
LD=gcc

LIBFLAGS=-Wl,-rpath
CFLAGS+=-Wall -O2
LFLAGS+=-lev

EET_OBJ=eet.o tty.o

eet: $(EET_OBJ)
	$(LD) $(LFLAGS) -o $@ $(EET_OBJ)

eet.o: eet.c eet.h tty.h

tty.o: tty.c tty.h
