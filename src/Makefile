CC = clang
# CC = gcc
# CFLAGS = -g -O0 -Wall
CFLAGS = -O2 -g -Wall -I ../deps/
LDFLAGS = -lbpf -lubpf -lm -lpthread -lrt

OBJS = servant.o sockets.o heart.o brain.o config.o service_stats.o log.o \
		map.o interpose_link.o vchannel.o userspace_maps.o \
		../deps/c-hashmap/map.o \
		udp_socket.o
HEADERS = $(wildcard ./*.h)
# $(info $(HEADERS))

all: servant

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

servant: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o servant $(LDFLAGS)

clean:
	rm -f $(OBJS) servant

INCDIR = /usr/include/servant
install:
	@echo "header install dir: $(INCDIR)"
	if [ ! -f /usr/bin/servant ]; then sudo ln -s $(CURDIR)/servant /usr/bin/servant; fi
	sudo mkdir -p $(INCDIR)
	sudo cp include/* $(INCDIR)
