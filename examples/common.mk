CLANG ?= clang-13
LLC ?= llc-13
CC := gcc
KERN_OBJECTS ?=
USER_OBJECTS ?=
CFLAGS := -g -O2 -Wall
LDFLAGS ?= -lbpf -lelf -lpthread $(USER_LIBS)
NOSTDINC_FLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include) -isystem /usr/local/include -isystem /usr/include
ARCH=$(shell uname -m | sed 's/x86_64/x86/' | sed 's/i386/x86/')
EXTRA_CFLAGS=-Werror
OUTDIR ?= ./

.PHONY: clean all outdir_check

all: outdir_check $(USER_OBJECTS) $(KERN_OBJECTS)

outdir_check:
	@-test ! -d $(OUTDIR) && mkdir -p $(OUTDIR)

clean:
	@find . -type f \
		\( -name '*~' \
		-o -name '*.ll' \
		-o -name '*.bc' \
		-o -name 'core' \) \
		-exec rm -vf '{}' \;
	@ for i in $(KERN_OBJECTS) $(USER_OBJECTS); do \
		rm -f $(OUTDIR)/$$i; \
	done

$(KERN_OBJECTS): %.o: %.c
	$(CLANG) -S $(NOSTDINC_FLAGS) $(LINUXINCLUDE) $(EXTRA_CFLAGS) \
		-D__KERNEL__ -D__ASM_SYSREG_H -D__BPF_TRACING__ \
		-DENABLE_ATOMICS_TESTS \
		-D__TARGET_ARCH_$(ARCH) \
		-Wno-unused-value -Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-Wno-gnu-variable-sized-type-not-at-end \
		-Wno-tautological-compare \
		-Wno-unknown-warning-option \
		-Wno-address-of-packed-member \
		-O2 -g -emit-llvm -c $< -o ${@:.o=.ll}
	$(LLC) -mcpu=v3 -march=bpf -filetype=obj -o "$(OUTDIR)/$@" ${@:.o=.ll}

$(USER_OBJECTS): %:%.c $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(OUTDIR)/$@ $< $(LDFLAGS)

