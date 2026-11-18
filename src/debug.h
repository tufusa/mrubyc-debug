#ifndef MRBC_SRC_DEBUG_H_
#define MRBC_SRC_DEBUG_H_

#define MRBC_USE_DEBUGGER

#if defined(MRBC_USE_DEBUGGER)
#include "vm.h"

typedef enum {
  RESUME_TYPE_STAY = 0,
  RESUME_TYPE_CONTINUE,
  RESUME_TYPE_STEP,
  RESUME_TYPE_NEXT,
  RESUME_TYPE_FINISH,
} debug_resume_type;

typedef struct BREAKPOINT {
  uint16_t filename_idx;
  uint16_t line;
  uint16_t id;
} mrbc_debug_breakpoint;

void debug_init(int (*read_bytes)(void *buf, int nbytes));
#endif

#endif
