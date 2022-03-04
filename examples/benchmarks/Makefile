KERN_OBJECTS = adjust_pkt_size.o echo.o memcpy.o xdp.o
USER_OBJECTS = report_tput loader
OUTDIR = bin/

ifdef XDP
EXTRA_CFLAGS = -Werror -D ISXDP
else ifdef UBPF
EXTRA_CFLAGS = -Werror -D ISUBPF
else
$(error "nothing selected! expecting XDP or UBPF")
endif

include ../common.mk
