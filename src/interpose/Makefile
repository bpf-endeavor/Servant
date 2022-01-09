CC = gcc
CFLAGS = -Wall -Werror -fPIC -g -O2 -shared

# Dependencies:
# * libbpf
# * libubpf compiled with -fPIC flag (place independent code)
# * pthread & dl
LIBS = -lbpf -lubpfPIC -lpthread -ldl -lrt

# Objects from Servant core code (previous directory ../)
SERVANT_OBJS = map.o brain.o log.o vchannel.o
# Objects specifically for interpose library
OTHER_OBJS =
ALL_OBJS = $(SERVANT_OBJS) $(OTHER_OBJS)

TARGET = servant_interpose.so
INSTALL_PATH = /usr/local/lib/

default: $(TARGET)

$(SERVANT_OBJS): %.o:../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OTHER_OBJS): %.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(ALL_OBJS) interpose.c
	$(CC) $(CFLAGS) $(ALL_OBJS) -o $@ interpose.c $(LIBS)

.PHONY: clean install uninstall

clean:
	rm -f $(ALL_OBJS)
	rm -f $(TARGET)

install: $(TARGET)
	install $(TARGET) $(INSTALL_PATH)

uninstall:
	rm -f $(INSTALL_PATH)/$(TARGET)

