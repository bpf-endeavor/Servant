#ifndef BRAIN_H
#define BRAIN_H

#include <ubpf.h>

// Maximum number of bytes a program file can have
#define MAX_CODE_SIZE 1024 * 1024 // 1 MB


/**
 * Create a ubpf engine for running the code defined by the program_path
 * parameter.
 *
 * @param program_path Path of the program to run
 * @param vm The pointer to vm will be set to the prepared object (it is an
 * OUTPUT parameter).
 *
 * @return zero on success
 */
int setup_ubpf_engine(char *program_path, struct ubpf_vm **vm);

/**
 * Run loaded eBPF code in the vm
 *
 * @param vm Pointer to virtual machine object.
 *
 * @return Return code from the eBPF program.
 */
int run_vm(struct ubpf_vm *vm, void *ctx, size_t ctx_len);
#endif
