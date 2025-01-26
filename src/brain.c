#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <elf.h>
#include <time.h>

#include "brain.h"
#include "log.h"
/* #include "config.h" */
#include "map.h"

static inline uint64_t
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

  static inline uint64_t __attribute__((always_inline))
ubpf_time_get_ns(void)
{
  struct timespec spec = {};
  /* clock_gettime(CLOCK_REALTIME, &spec); */
  clock_gettime(CLOCK_MONOTONIC, &spec);
  /* clock_gettime(CLOCK_MONOTONIC_COARSE, &spec); */
  return (uint64_t)(spec.tv_sec) * (uint64_t)1000000000LL +
    (uint64_t)(spec.tv_nsec);
}

typedef struct {
  char *name;
  void *fn;
} helper_t;

/**
 * Register the supported functions in the virtual machine
 */
  static void
register_engine_functions(struct ubpf_vm *vm)
{
  helper_t list[] = {
    /* Access Kernel maps */
    {"_kern_lookup", ubpf_map_lookup_elem_kern},
    {"_kern_fast_lookup", ubpf_map_lookup_elem_kern_fast},
    /* printf for debugging */
    {"printf", printf},
    /* get the CPU timestamp counter */
    {"rdtsc", readTSC},
    /* memmove */
    {"ubpf_memmove", memmove},
    /* Userspace maps (From uBPF library) */
    {"_userspace_lookup", ubpf_lookup_map},
    {"_userspace_update", ubpf_update_map},
    /* get time in ns */
    {"ubpf_time_get_ns", ubpf_time_get_ns},
    /* Two phase lookup */
    {"_userspace_p1", ubpf_lookup_map_p1},
    {"_userspace_p2", ubpf_lookup_map_p2},
  };
  const size_t cnt = sizeof(list) /  sizeof(helper_t);
  int i = 0, ret;
  for (; i < cnt; i++) {
    helper_t *h = &list[i];
    ret = ubpf_register(vm, i+1, h->name, h->fn);
    if (ret != 0) {
      ERROR("Failed to register helper funciton with VM (%s)\n", h->name);
      return;
    }
  }
  /* unwind */ /* NOTE: keep this the last one */
  ret = ubpf_register(vm, i+1, "unwind", unwind);
  if (ret != 0) {
    ERROR("Failed to register unwind function\n");
    return;
  }
  ubpf_set_unwind_function_index(vm, i+1);
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
    INFO("Loading an ELF File ...\n");
    ret = ubpf_load_elf(vm, code, code_len, &errmsg);
  } else {
    DEBUG("It is not an ELF file, maybe check it ...\n");
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
  INFO("--- ELF loaded\n");
  *_vm = vm;

  /**
   * This part of code dumps the jitted code
   * it is used for debugging ubpf interpretter
   */
  /* if (config.jitted) { */
  /* 	ubpf_jit_fn fn = ubpf_compile(vm, 0, &errmsg); */
  /* 	if (fn == NULL) { */
  /* 		ERROR("Failed to compile: %s\n", errmsg); */
  /* 		free(errmsg); */
  /* 		return 1; */
  /* 	} */
  /* 	/1* dump the jitted program *1/ */
  /* 	unsigned int size = 0; */
  /* 	uint8_t *b = ubpf_dump_jitted_fn(vm, &size); */
  /* 	for (int i = 0; i < size; i++) { */
  /* 		if (i % 16 == 0) */
  /* 			printf("\n"); */
  /* 		printf("%.2x ", b[i]); */
  /* 	} */
  /* 	printf("\n"); */
  /* } */
  return 0;
}
