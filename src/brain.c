#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <elf.h>
#include "brain.h"
#include "log.h"
#include "config.h"
#include "map.h"

static uint64_t
unwind(uint64_t i)
{
    return i;
}

# include <x86intrin.h>
static inline
uint64_t readTSC() {
    // _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
    uint64_t tsc = __rdtsc();
    // _mm_lfence();  // optionally block later instructions until rdtsc retires
    return tsc;
}

static inline void
_memmove(void *d, void *s, uint32_t n)
{
	memmove(d, s, n);
}

static inline void * __attribute__((always_inline))
_ubpf_lookup_map(struct ubpf_map *m, void *k)
{
	return ubpf_lookup_map(m, k);
}

static inline int __attribute__((always_inline))
_ubpf_update_map(struct ubpf_map *m, void *k, void *v)
{
	return ubpf_update_map(m, k, v);
}

/**
 * Register the supported functions in the virtual machine
 */
static void
register_engine_functions(struct ubpf_vm *vm)
{
	/* Access Kernel maps */
	ubpf_register(vm, 1, "ubpf_map_lookup_elem_kern", ubpf_map_lookup_elem_kern);
	ubpf_register(vm, 2, "ubpf_map_update_elem_kern", ubpf_map_update_elem_kern);
	ubpf_register(vm, 3, "ubpf_map_elem_release", ubpf_map_elem_release);
	/* printf for debugging */
	ubpf_register(vm, 4, "printf", printf);
	/* get the CPU timestamp counter */
	ubpf_register(vm, 5, "rdtsc", readTSC);
	/* memmove */
	ubpf_register(vm, 6, "ubpf_memmove", _memmove);
	/* Userspace maps (From uBPF library) */
	ubpf_register(vm, 7, "ubpf_map_lookup_elem_userspace", _ubpf_lookup_map);
	ubpf_register(vm, 8, "ubpf_map_update_elem_userspace", _ubpf_update_map);
	/* unwind */
	ubpf_register(vm, 9, "unwind", unwind);
	ubpf_set_unwind_function_index(vm, 9);
}

/**
 * Load the program code.
 */
static void *
readfile(const char *path, size_t maxlen, size_t *len)
{
	FILE *file;
	file = fopen(path, "r");

	if (file == NULL) {
		ERROR("Failed to open %s: %s\n", path, strerror(errno));
		return NULL;
	}

	void *data = calloc(maxlen, 1);
	size_t offset = 0;
	/* size_t ret; */
	int rv;
	while ((rv = fread(data+offset, 1, maxlen-offset, file)) > 0) {
		offset += rv;
	}

	if (ferror(file)) {
		ERROR("Failed to read %s: %s\n", path, strerror(errno));
		fclose(file);
		free(data);
		return NULL;
	}

	if (!feof(file)) {
		ERROR("Failed to read %s because it is too large (max %u bytes)\n",
				path, (unsigned)maxlen);
		fclose(file);
		free(data);
		return NULL;
	}

	fclose(file);
	if (len) {
		*len = offset;
	}
	return data;
}

int
setup_ubpf_engine(char *program_path, struct ubpf_vm **_vm)
{
	size_t code_len;
	void *code = readfile(program_path, MAX_CODE_SIZE, &code_len);
	if (code == NULL) {
		return 1;
	}
	char *errmsg;
	int ret;
	struct ubpf_vm *vm = ubpf_create();
	if (!vm) {
		ERROR("Failed to create uBPF VM\n");
		return 1;
	}
	register_engine_functions(vm);
	ubpf_toggle_bounds_check(vm, false);
	/*
	 * The ELF magic corresponds to an RSH instruction with an offset,
	 * which is invalid.
	 */
	bool elf = code_len >= SELFMAG && !memcmp(code, ELFMAG, SELFMAG);
	if (elf) {
		ret = ubpf_load_elf(vm, code, code_len, &errmsg);
	} else {
		ret = ubpf_load(vm, code, code_len, &errmsg);
	}
	// We do not need the code data any more
	free(code);
	if (ret < 0) {
		ERROR("Failed to load code: %s\n", errmsg);
		free(errmsg);
		ubpf_destroy(vm);
		return 1;
	}
	*_vm = vm;

	/**
	 * This part of code dumps the jitted code
	 * it is used for debugging ubpf interpretter
	 */
	if (config.jitted) {
		ubpf_jit_fn fn = ubpf_compile(vm, &errmsg);
		if (fn == NULL) {
			ERROR("Failed to compile: %s\n", errmsg);
			free(errmsg);
			return 1;
		}
		/* dump the jitted program */
		/* unsigned int size = 0; */
		/* uint8_t *b = ubpf_dump_jitted_fn(vm, &size); */
		/* for (int i = 0; i < size; i++) { */
		/* 	if (i % 16 == 0) */
		/* 		printf("\n"); */
		/* 	printf("%.2x ", b[i]); */
		/* } */
		/* printf("\n"); */
	}
	return 0;
}


/* #define delay_cycles 0 */
/* static int fn(void *ctx, size_t ctx_len) */
/* { */
/* 	uint64_t begin = readTSC(); */
/* 	uint64_t now = begin; */
/* 	uint64_t end = now + delay_cycles; */
/* 	if (now <= end) { */
/* 		while (now  < end) { */
/* 			now = readTSC(); */
/* 		} */
/* 	} else { */
/* 		while (now > end) { */
/* 			now = readTSC(); */
/* 		} */
/* 		while (now < end) { */
/* 			now = readTSC(); */
/* 		} */
/* 	} */
/* 	// 200 Drop */
/* 	// 300 Send */
/* 	return 300; */
/* } */

/* int */
/* run_vm(struct ubpf_vm *vm, void *ctx, size_t ctx_len) */
/* { */
/* 	uint64_t ret; */
/* 	char *errmsg; */
/* 	// TODO: Maybe it is better to selected either jitted or non-jitted */
/* 	// approach and remove the if statement. */
/* 	if (config.jitted) { */
/* 		ubpf_jit_fn fn = ubpf_compile(vm, &errmsg); */
/* 		if (fn == NULL) { */
/* 			ERROR("Failed to compile: %s\n", errmsg); */
/* 			free(errmsg); */
/* 			return 1; */
/* 		} */
/* 		ret = fn(ctx, ctx_len); */
/* 	} else { */
/* 		if (ubpf_exec(vm, ctx, ctx_len, &ret) < 0) */
/* 			ret = UINT64_MAX; */
/* 	} */
/* 	/1* DEBUG("ubpf ret: %d\n", ret); *1/ */
/* 	return ret; */
/* } */

