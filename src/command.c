#include <stdbool.h>
#include <stddef.h>

#include "console.h"

#include "breakpoint.h"
#include "common.h"
#include "command.h"

#define BUFFER_LEN 256

static char buf[BUFFER_LEN] = {0};

static int parse_integer(char *integer, int len) {
  if(len == 0) return -1;

  int base = 1;
  int res = 0;
  for(int i = len - 1; i >= 0; i--) {
    int digit = integer[i] - '0';
    if(digit < 0 || 10 < digit) return -1;

    res = digit * base;
    base *= 10;
  }
  return res;
}

static int get_basename(char *filename, int filename_len, char **basename) {
  int offset = 0;
  for(int i = 0; i < filename_len; i++) {
    if(filename[i] == '/') offset = i + 1;
  }
  *basename = filename + offset;
  return filename_len - offset;
}

static int find_filename_idx(mrbc_vm *vm, char *filename, int filename_len) {
  char *basename = NULL;
  int basename_len = get_basename(filename, filename_len, &basename);
  for(uint16_t idx = 0; idx < vm->debug_filenames_num; idx++) {
    int len = strlen(vm->debug_filenames[idx]);
    char *bname = NULL;
    int bname_len = get_basename(vm->debug_filenames[idx], len, &bname);

    if(basename_len == bname_len && strncmp(basename, bname, basename_len) == 0) return idx;
  }
  return -1;
}

static void print_breakpoint(const mrbc_debug_breakpoint *breakpoint, void *ctx) {
  mrbc_vm *vm = ctx;
  mrbc_printf("#%d %s:%d\n", breakpoint->id, vm->debug_filenames[breakpoint->filename_idx], breakpoint->line);
}

static bool command_break(mrbc_vm *vm, char *args, int len) {
  if(len == 0) { // `break`
    foreach_breakpoints(print_breakpoint, vm);
    return true;
  }

  if(*args == ' ') {
    args++;
    len--;
  } else {
    return false;
  }

  int filename_len = -1;
  for(int idx = 0; idx < len; idx++) {
    if(args[idx] == ':') {
      filename_len = idx;
      break;
    }
  }

  int filename_idx = -1;
  if(filename_len == -1) { // `break <line>`
    filename_idx = 0;
  } else { // `break <file>:<line>`
    filename_idx = find_filename_idx(vm, args, filename_len);
    if(filename_idx == -1) return false;

    args += filename_len + 1;
    len -= filename_len + 1;
  }

  int line = parse_integer(args, len);
  if(line == -1) return false;

  const mrbc_debug_breakpoint *breakpoint = add_breakpoint((uint16_t)filename_idx, (uint16_t)line);
  if(breakpoint == NULL) return false;

  mrbc_printf("Breakpoint %d added at %s:%d\n", breakpoint->id, vm->debug_filenames[breakpoint->filename_idx], breakpoint->line);

  return true;
}

static bool command_delete(mrbc_vm *vm, char *args, int len) {
  if(len == 0) {
    return delete_all_breakpoints();
  }

  if(*args == ' ') {
    args++;
    len--;
  } else {
    return false;
  }

  int id = parse_integer(args, len);
  if(id == -1) return false;

  return delete_breakpoint(id);
}

static bool command_info(mrbc_vm *vm, char *args, int len) {
  if(len != 0) return false;

  for(int i = 0; i < vm->cur_irep->nlocals - 1; i++) {
    uint16_t id = vm->cur_irep->lvar_sym_ids[i];
    if(id == 0xFFFF) continue;

    char *name = vm->lvar_symbols[id];
    mrbc_printf("%s = ", name);
    mrbc_p_sub(&vm->cur_regs[i + 1]);
    mrbc_printf("\n");
  }
  return true;
}

static void trace_callinfo(mrbc_vm *vm, mrbc_callinfo *callinfo, int depth, void (*func)(int depth, const char *filename, int line)) {
  if(callinfo == NULL) return;

  const mrbc_irep *irep = callinfo->cur_irep;
  if(!irep->debug_info) return;

  // callinfo->infoはリターンアドレスなので1バイト戻す オペコードではない可能性が高いがpcの探索は範囲なので問題なし
  uint32_t pc = (callinfo->inst - 1) - irep->inst;
  int line = debug_find_line(irep, pc);
  uint16_t filename_idx = debug_find_filename_idx(irep, pc);

  if(func) func(depth, vm->debug_filenames[filename_idx], line);

  trace_callinfo(vm, callinfo->prev, depth + 1, func);
}

static void print_backtrace(int depth, const char *filename, int line) {
  mrbc_printf("#%d %s:%d\n", depth, filename, line);
}

static bool command_backtrace(mrbc_vm *vm, char *args, int len) {
  if(len != 0) return false;

  trace_callinfo(vm, vm->callinfo_tail, 0, print_backtrace);
  return true;
}

debug_resume_type handle_command(mrbc_vm *vm, char *command, int len) {
  if(len == 8 && strncmp(command, "continue", 8) == 0) {
    return RESUME_TYPE_CONTINUE;
  }
  else if(len == 1 && strncmp(command, "c", 1) == 0) {
    return RESUME_TYPE_CONTINUE;
  }
  else if(len == 4 && strncmp(command, "step", 4) == 0) {
    return RESUME_TYPE_STEP;
  }
  else if(len == 1 && strncmp(command, "s", 1) == 0) {
    return RESUME_TYPE_STEP;
  }
  else if(len == 4 && strncmp(command, "next", 4) == 0) {
    // TODO: `next`
    return RESUME_TYPE_STEP;
  }
  else if(len == 1 && strncmp(command, "n", 1) == 0) {
    // TODO: `next`
    return RESUME_TYPE_STEP;
  }
  else if(len == 6 && strncmp(command, "finish", 6) == 0) {
    // TODO: `finish`
    return RESUME_TYPE_STEP;
  }
  else if(len == 3 && strncmp(command, "fin", 3) == 0) {
    // TODO: `finish`
    return RESUME_TYPE_STEP;
  }
  else if(len >= 5 && strncmp(command, "break", 5) == 0) {
    command_break(vm, command + 5, len - 5);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 1 && strncmp(command, "b", 1) == 0) {
    // FIXME: backtraceをキャッチしてしまう, 前方一致はやめたほうがよさそうか
    command_break(vm, command + 1, len - 1);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 6 && strncmp(command, "delete", 6) == 0) {
    command_delete(vm, command + 6, len - 6);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 1 && strncmp(command, "d", 1) == 0) {
    command_delete(vm, command + 1, len - 1);
    return RESUME_TYPE_STAY;
  }
  else if(len == 4 && strncmp(command, "info", 4) == 0) {
    command_info(vm, command + 4, len - 4);
    return RESUME_TYPE_STAY;
  }
  else if(len == 9 && strncmp(command, "backtrace", 9) == 0) {
    command_backtrace(vm, command + 9, len - 9);
    return RESUME_TYPE_STAY;
  }
  else if(len == 2 && strncmp(command, "bt", 2) == 0) {
    command_backtrace(vm, command + 2, len - 2);
    return RESUME_TYPE_STAY;
  }

  return RESUME_TYPE_STAY;
}
