CC = clang-11
CFLAGS = -g
LDFLAGS = -lbpf -lm -lpthread

OBJS = servant.o sockets.o heart.o config.o service_stats.o log.o
HEADERS = $(wildcard ./*.h)
$(info $(HEADERS))

all: servant

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c -o $@ $<

servant: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o servant

clean:
	rm -f *.o servant
