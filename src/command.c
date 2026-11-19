#include <stdbool.h>
#include <stddef.h>

#include "console.h"

#include "breakpoint.h"
#include "common.h"
#include "command.h"

typedef struct {
  const char *name;
  int len;
  debug_resume_type (*handler)(mrbc_vm *vm, char *args, int len);
} command_entry;

#define COMMAND_ENTRY(name, handler) (command_entry){ name, sizeof(name)-1, handler }

static int parse_integer(char *integer, int len) {
  if(len == 0) return -1;

  int base = 1;
  int res = 0;
  for(int i = len - 1; i >= 0; i--) {
    int digit = integer[i] - '0';
    if(digit < 0 || 10 < digit) return -1;

    res += digit * base;
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

static debug_resume_type handle_command_continue(mrbc_vm *vm, char *args, int len) { return RESUME_TYPE_CONTINUE; }
static debug_resume_type handle_command_step(mrbc_vm *vm, char *args, int len) { return RESUME_TYPE_STEP; }
static debug_resume_type handle_command_next(mrbc_vm *vm, char *args, int len) { return RESUME_TYPE_STEP; }
static debug_resume_type handle_command_finish(mrbc_vm *vm, char *args, int len) { return RESUME_TYPE_STEP; }
static debug_resume_type handle_command_break(mrbc_vm *vm, char *args, int len) { command_break(vm, args, len); return RESUME_TYPE_STAY; }
static debug_resume_type handle_command_delete(mrbc_vm *vm, char *args, int len) { command_delete(vm, args, len); return RESUME_TYPE_STAY; }
static debug_resume_type handle_command_info(mrbc_vm *vm, char *args, int len) { command_info(vm, args, len); return RESUME_TYPE_STAY; }
static debug_resume_type handle_command_backtrace(mrbc_vm *vm, char *args, int len) { command_backtrace(vm, args, len); return RESUME_TYPE_STAY; }

static const command_entry commands[] = {
  COMMAND_ENTRY("continue", handle_command_continue),
  COMMAND_ENTRY("c", handle_command_continue),
  COMMAND_ENTRY("step", handle_command_step),
  COMMAND_ENTRY("s", handle_command_step),
  COMMAND_ENTRY("next", handle_command_next),
  COMMAND_ENTRY("n", handle_command_next),
  COMMAND_ENTRY("finish", handle_command_finish),
  COMMAND_ENTRY("fin", handle_command_finish),
  COMMAND_ENTRY("break", handle_command_break),
  COMMAND_ENTRY("b", handle_command_break),
  COMMAND_ENTRY("delete", handle_command_delete),
  COMMAND_ENTRY("d", handle_command_delete),
  COMMAND_ENTRY("info", handle_command_info),
  COMMAND_ENTRY("i", handle_command_info),
  COMMAND_ENTRY("backtrace", handle_command_backtrace),
  COMMAND_ENTRY("bt", handle_command_backtrace),
};

debug_resume_type handle_command(mrbc_vm *vm, char *command, int len) {
  int command_len = 0;
  while(command_len < len && command[command_len] != ' ') command_len++; // 最初の空白までがコマンド

  int args_len = len - command_len;
  while(args_len > 0 && command[len - args_len] == ' ') args_len--; // 先頭の空白を無視

  char* args = command + (len - args_len);

  for(int i = 0; i < sizeof(commands) / sizeof(command_entry); i++) {
    if(command_len == commands[i].len && strncmp(command, commands[i].name, commands[i].len) == 0 && commands[i].handler) {
      return commands[i].handler(vm, args, args_len);
    }
  }

  return RESUME_TYPE_STAY;
}
