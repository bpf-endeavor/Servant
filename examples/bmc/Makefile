LLC = llc-11
CLANG = clang-11
UBPF_OBJ = ubpf.o
XDP_OBJ = xdp.o

BPF_CFLAGS ?= -Wall \
		-Wno-unused-value \
		-Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-Werror \
		-O2


all: $(XDP_OBJ) $(UBPF_OBJ)

$(UBPF_OBJ): %.o: %.c
	$(CLANG) -target bpf $(BPF_CFLAGS) -c $< -o $@

$(XDP_OBJ): %.o: %.c Makefile
	$(CLANG) -S \
		-target bpf \
		-D __BPF_TRACING__ \
		$(BPF_CFLAGS) \
		-emit-llvm -c -g -o ${@:.o=.ll} $<
	$(LLC) -march=bpf -mcpu=probe -filetype=obj -o $@ ${@:.o=.ll}

clean:
	rm -rf $(XDP_OBJ) $(UBPF_OBJ)

