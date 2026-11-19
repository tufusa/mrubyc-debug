#include <stddef.h>

#include "alloc.h"

#include "breakpoint.h"

static mrbc_debug_breakpoint *breakpoints = NULL;
static int breakpoint_num = 0;
static int breakpoints_max_len = 0;
static uint16_t next_breakpoint_id = 0;

const mrbc_debug_breakpoint *add_breakpoint(uint16_t filename_idx, uint16_t line) {
  if(breakpoint_num == breakpoints_max_len) {
    int new_max_len = breakpoints_max_len == 0 ? 8 : breakpoints_max_len * 2; // 初期は8, 以降は2倍
    mrbc_debug_breakpoint *new_breakpoints = (mrbc_debug_breakpoint*)mrbc_raw_realloc(breakpoints, new_max_len * sizeof(mrbc_debug_breakpoint));
    if(new_breakpoints == NULL) return NULL;
    breakpoints = new_breakpoints;
    breakpoints_max_len = new_max_len;
  }

  uint16_t id = next_breakpoint_id++;
  breakpoints[breakpoint_num] = (mrbc_debug_breakpoint){ filename_idx, line, id };
  return &breakpoints[breakpoint_num++];
}

bool delete_breakpoint(uint16_t id) {
  int idx = -1;
  for(int i = 0; i < breakpoint_num; i++) {
    if(breakpoints[i].id == id) {
      idx = i;
      break;
    }
  }
  if(idx == -1) return false;

  breakpoints[idx] = breakpoints[--breakpoint_num];
  return true;
}

bool delete_all_breakpoints() {
  if(breakpoints) mrbc_raw_free(breakpoints);
  breakpoints = NULL;
  breakpoint_num = 0;
  breakpoints_max_len = 0;
  return true;
}

void foreach_breakpoints(void (*func)(const mrbc_debug_breakpoint *breakpoint, void *ctx), void *ctx) {
  for(int i = 0; i < breakpoint_num; i++) {
    if(func) func(&breakpoints[i], ctx);
  }
}

const mrbc_debug_breakpoint *find_breakpoint(uint16_t filename_idx, uint16_t line) {
  for(int i = 0; i < breakpoint_num; i++) {
    if(filename_idx == breakpoints[i].filename_idx && line == breakpoints[i].line) {
      return &breakpoints[i];
    }
  }
  return NULL;
}
