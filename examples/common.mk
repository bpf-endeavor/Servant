DIR := $(dir $(lastword $(MAKEFILE_LIST)))
LIBBPF_INSTALL_DIR="$(DIR)/../_deps/libbpf/src/build/usr/"
CLANG ?= clang
LLC ?= llc
CC := gcc
KERN_OBJECTS ?=
USER_OBJECTS ?=
CFLAGS := -g -O2 -Wall -I "$(LIBBPF_INSTALL_DIR)/include"
LDFLAGS ?= -L "$(LIBBPF_INSTALL_DIR)/lib64" -lbpf -lelf -lpthread $(USER_LIBS)
NOSTDINC_FLAGS := -nostdinc \
				  -isystem $(shell $(CC) -print-file-name=include) \
				  -isystem /usr/local/include \
				  -isystem /usr/include \
				  -isystem /usr/include/x86_64-linux-gnu \
				  -I "$(LIBBPF_INSTALL_DIR)/include"
ARCH=$(shell uname -m | sed 's/x86_64/x86/' | sed 's/i386/x86/')
EXTRA_CFLAGS ?= -Werror
OUTDIR ?= ./

_KERN_OBJECTS = $(addprefix $(OUTDIR), $(KERN_OBJECTS))
_USER_OBJECTS = $(addprefix $(OUTDIR), $(USER_OBJECTS))

.PHONY: clean all outdir_check

all: outdir_check $(_USER_OBJECTS) $(_KERN_OBJECTS)

outdir_check:
	@test ! -d $(OUTDIR) && mkdir -p $(OUTDIR) || echo "1" > /dev/null

clean:
	@find . -type f \
		\( -name '*~' \
		-o -name '*.ll' \
		-o -name '*.bc' \
		-o -name 'core' \) \
		-exec rm -vf '{}' \;
	rm -f $(_KERN_OBJECTS) $(_USER_OBJECTS)

$(_KERN_OBJECTS): $(OUTDIR)%.o:%.c
	$(CLANG) -S $(NOSTDINC_FLAGS) $(EXTRA_CFLAGS) \
		-D__KERNEL__ -D__ASM_SYSREG_H -D__BPF_TRACING__ \
		-DENABLE_ATOMICS_TESTS \
		-D__TARGET_ARCH_$(ARCH) \
		-Wno-unused-value -Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-Wno-gnu-variable-sized-type-not-at-end \
		-Wno-tautological-compare \
		-Wno-unknown-warning-option \
		-Wno-address-of-packed-member \
		-O2 -g -emit-llvm -c $< -o - | $(LLC) -mcpu=v3 -march=bpf -filetype=obj -o "$@"

$(_USER_OBJECTS): $(OUTDIR)%:%.c $(OBJECTS)
	echo $@ $<
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $< $(LDFLAGS)
