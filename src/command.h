#ifndef MRBC_DEBUG_SRC_COMMAND_H_
#define MRBC_DEBUG_SRC_COMMAND_H_

#include "vm.h"

#include "debug.h"

debug_resume_type handle_command(mrbc_vm *vm, char *command, int len);

#endif
