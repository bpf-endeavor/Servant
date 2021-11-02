# CC = clang-11
CC = gcc
# CFLAGS = -g -O0 -Wall
CFLAGS = -O2 -Wall
LDFLAGS = -lbpf -lubpf -lm -lpthread

OBJS = servant.o sockets.o heart.o brain.o config.o service_stats.o log.o map.o
HEADERS = $(wildcard ./*.h)
# $(info $(HEADERS))

all: servant

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

servant: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o servant $(LDFLAGS)

clean:
	rm -f $(OBJS) servant
