LLC = llc-9
CLANG = clang-9
UBPF_OBJ = ubpf.o bmc_in_ubpf.o
XDP_OBJ = xdp.o

# -Werror
BPF_CFLAGS ?= -Wall \
		-Wno-unused-value \
		-Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-O2

NOSTDINC_FLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include)
ARCH=$(shell uname -m | sed 's/x86_64/x86/' | sed 's/i386/x86/')
EXTRA_CFLAGS=-Werror


all: $(XDP_OBJ) $(UBPF_OBJ)

$(UBPF_OBJ): %.o: %.c
	# $(CLANG) -target bpf $(BPF_CFLAGS) -c $< -o $@ 
	$(CLANG) -S $(NOSTDINC_FLAGS) $(LINUXINCLUDE) $(EXTRA_CFLAGS) \
	    -D__KERNEL__ -D__ASM_SYSREG_H -D__BPF_TRACING__ \
	    -D__TARGET_ARCH_$(ARCH) \
	    -Wno-unused-value -Wno-pointer-sign \
	    -Wno-compare-distinct-pointer-types \
	    -Wno-gnu-variable-sized-type-not-at-end \
	    -Wno-tautological-compare \
	    -Wno-unknown-warning-option \
	    -Wno-address-of-packed-member \
	    -O2 -g -emit-llvm -c $< -o ${@:.o=.ll}
	$(LLC) -march=bpf -filetype=obj -o $@ ${@:.o=.ll}

$(XDP_OBJ): %.o: %.c Makefile
	$(CLANG) -S \
		-target bpf \
		-D __BPF_TRACING__ \
		$(BPF_CFLAGS) \
		-emit-llvm -c -g -o ${@:.o=.ll} $<
	$(LLC) -march=bpf -mcpu=probe -filetype=obj -o $@ ${@:.o=.ll}

clean:
	rm -rf $(XDP_OBJ) $(UBPF_OBJ)

