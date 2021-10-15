CC = clang-11
CFLAGS = -g -O2 -Wall
LDFLAGS = -lbpf -lubpf -lm -lpthread

OBJS = servant.o sockets.o heart.o brain.o config.o service_stats.o log.o map.o
HEADERS = $(wildcard ./*.h)
# $(info $(HEADERS))

all: servant

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c -o $@ $<

servant: $(OBJS)
	$(CC) $(OBJS) -o servant $(LDFLAGS)

clean:
	rm -f $(OBJS) servant
