OUTDIR = ./bin/
KERN_OBJECTS = adjust_pkt_size.o echo.o memcpy.o xdp.o test_inc_pkt_head.o \
			state_overhead.o payload.o loop.o packet_processing.o \
			tail_call.o hash_map.o memmove.o
USER_OBJECTS = report_tput loader

ifdef XDP
EXTRA_CFLAGS = -Werror -D ISXDP
else ifdef UBPF
EXTRA_CFLAGS = -Werror -D ISUBPF
# NOTE: for memmove experiment add this to the flag: -fno-builtin
ifdef COPY_STATE
	EXTRA_CFLAGS += -D COPY_STATE=$(COPY_STATE)
endif
else
ifneq (clean, $(MAKECMDGOALS))
$(error "nothing selected! expecting XDP or UBPF")
endif
endif

include ../common.mk
