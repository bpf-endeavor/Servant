UNAME=$(shell uname)

CCFLAGS=-Wall -Wextra -Wconversion -Wredundant-decls -Wshadow -Wno-unused-parameter -O3
CC=clang-11

FILES = test_log.o
DEPS = ../src/log.c

all: test

remake: clean all

%.o: %.c ctest.h
	$(CC) $(CCFLAGS) -c -o $@ $<

test: main.o ctest.h $(FILES) $(DEPS)
	$(CC) $(LDFLAGS) main.o $(FILES) $(DEPS) -o test

clean:
	rm -f test *.o

