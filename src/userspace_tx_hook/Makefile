CC = gcc
CFLAGS = -Wall -Werror -fPIC -g -O2 -shared
LIBS = -lbpf -lubpfPIC -lpthread -ldl
OBJS = map.o brain.o log.o
TARGET = servant_interpose.so
INSTALL_PATH = /usr/local/lib/

default: $(TARGET)

%.o: ../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) interpose.c
	$(CC) $(CFLAGS) $(OBJS) -o $@ interpose.c $(LIBS)

.PHONY: clean install uninstall

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install: $(TARGET)
	install $(TARGET) $(INSTALL_PATH)

uninstall:
	rm -f $(INSTALL_PATH)/$(TARGET)
