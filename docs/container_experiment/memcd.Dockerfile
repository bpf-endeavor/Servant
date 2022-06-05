FROM ubuntu:20.04

WORKDIR /root/

RUN apt-get update && apt-get install -y git clang-9 automake libevent-dev build-essential autoconf make libc-dev;
RUN git clone https://github.com/Orange-OpenSource/bmc-cache.git; \
	cd bmc-cache/memcached-sr; \
	./autogen.sh; \
	CC=clang-9 CFLAGS='-DREUSEPORT_OPT=1 -Wno-deprecated-declarations' ./configure && make

CMD ["/root/bmc-cache/memcached-sr/memcached", "-u", "root", "-c", "4096", "-p", "11211", "-U", "8080", "-m", "5120", "-t" , "1", "-o", "reuseport"]
