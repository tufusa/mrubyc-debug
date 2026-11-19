#include "mrubyc.h"

#include "breakpoint.h"
#include "command.h"
#include "input.h"
#include "common.h"
#include "debug.h"

static int prev_line = 0;
static debug_resume_type prev_resume_type = RESUME_TYPE_STAY;

static debug_resume_type handle_input(mrbc_vm* vm) {
  const int max_len = 256;
  char buf[256] = {0}; // FIXME
  debug_resume_type resume_type = RESUME_TYPE_STAY;

  while (resume_type == RESUME_TYPE_STAY) {
    mrbc_print("(mrcdbg) ");

    int len = read_line(buf, max_len);
    resume_type = handle_command(vm, buf, len);
  }
  return resume_type;
}

int trace_irep_tree_sub(mrbc_irep *top, int index, int depth) {
  for(int d = 0; d < depth; d++) mrbc_print(" ");
  mrbc_printf("#%d: %p %d %d %d\n", index, top, top->inst[0], top->inst[1], top->inst[2]);

  mrbc_irep *irep;
  for(int i = 0; i < top->rlen; i++) {
    irep = mrbc_irep_child_irep(top, i);
    index = trace_irep_tree_sub(irep, index + 1, depth + 1);
  }
  return index;
}

int trace_irep_tree(mrbc_irep* top) {
  trace_irep_tree_sub(top, 0, 0);
  mrbc_hal_flush(1);
  return 0;
}

static void debug_on_code_fetch(mrbc_vm *vm, const mrbc_irep *irep, const uint16_t pc) {
  if(!irep->debug_info) return;

  int line = debug_find_line(vm->cur_irep, pc);
  if(prev_line == line) return;

  uint16_t filename_idx = debug_find_filename_idx(irep, pc);
  int stop = false;
  if(prev_line == 0 || prev_resume_type == RESUME_TYPE_STEP) {
    stop = true;
    mrbc_printf("Stop: %s:%d\n", vm->debug_filenames[filename_idx], line);
  }

  if(prev_resume_type == RESUME_TYPE_CONTINUE) {
    const mrbc_debug_breakpoint *breakpoint = find_breakpoint(filename_idx, line);
    if(breakpoint) {
      stop = true;
      mrbc_printf("Breakpoint %d: %s:%d\n", breakpoint->id, vm->debug_filenames[breakpoint->filename_idx], breakpoint->line);
    }
  }

  if(stop) {
    prev_resume_type = handle_input(vm);
  }

  prev_line = line;
}

void debug_init(int (*read)(void *buf, int nbytes)) {
  set_read_bytes(read);
  mrbc_set_code_fetch_hook(debug_on_code_fetch);
}
