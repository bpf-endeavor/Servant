OUTDIR = ./bin/
KERN_OBJECTS = adjust_pkt_size.o echo.o memcpy.o xdp.o test_inc_pkt_head.o
USER_OBJECTS = report_tput loader

ifdef XDP
EXTRA_CFLAGS = -Werror -D ISXDP
else ifdef UBPF
EXTRA_CFLAGS = -Werror -D ISUBPF
else
ifneq (clean, $(MAKECMDGOALS))
$(error "nothing selected! expecting XDP or UBPF")
endif
endif

include ../common.mk
