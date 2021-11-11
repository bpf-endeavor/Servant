LLC = llc-9
CLANG = clang-9
UBPF_OBJ = ubpf.o
XDP_OBJ = xdp.o
LOADER = loader

all: $(XDP_OBJ) $(UBPF_OBJ) $(LOADER)

$(UBPF_OBJ) $(XDP_OBJ): %.o: %.c Makefile
	$(CLANG) -S \
		-target bpf \
		-D __BPF_TRACING__ \
		-Wall \
		-Wno-unused-value \
		-Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-Werror \
		-O2 -emit-llvm -c -g -o ${@:.o=.ll} $<
	$(LLC) -march=bpf -mcpu=probe -filetype=obj -o $@ ${@:.o=.ll}

$(LOADER):
	$(CLANG) -O2 -Wall -Werror $(LOADER).c -o $@ -lbpf

clean:
	rm -rf $(XDP_OBJ) $(UBPF_OBJ) ${XDP_OBJ:.o=.ll} ${UBPF_OBJ:.o=.ll} $(LOADER)

