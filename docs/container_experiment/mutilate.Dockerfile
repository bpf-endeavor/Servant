FROM ubuntu:20.04
WORKDIR /root/
RUN apt-get update && apt-get install -y git scons libevent-dev gengetopt libzmq3-dev build-essential libc-dev;
RUN git clone https://github.com/fshahinfar1/mutilate.git; \
	cd mutilate; \
	mv SConstruct3 SConstruct; \
	scons > /dev/null 2>&1;
COPY ./stress.sh ./stress.sh


# Run this container in interactive mode and issue the execution command!
CMD ["/bin/bash"]
