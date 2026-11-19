#ifndef MRBC_DEBUG_SRC_BREAKPOINT_H_
#define MRBC_DEBUG_SRC_BREAKPOINT_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct BREAKPOINT {
  uint16_t filename_idx;
  uint16_t line;
  uint16_t id;
} mrbc_debug_breakpoint;

const mrbc_debug_breakpoint *add_breakpoint(uint16_t filename_idx, uint16_t line);

bool delete_breakpoint(uint16_t id);

bool delete_all_breakpoints();

void foreach_breakpoints(void (*func)(const mrbc_debug_breakpoint *breakpoint, void *ctx), void *ctx);

const mrbc_debug_breakpoint *find_breakpoint(uint16_t filename_idx, uint16_t line);

#endif