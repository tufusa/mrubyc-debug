#include "mrubyc.h"
#include "debug.h"

#if defined(MRBC_USE_DEBUGGER)

static int (*read_bytes)(void *buf, int nbytes) = NULL;

static int prev_line = 0;
static debug_resume_type prev_resume_type = RESUME_TYPE_STAY;

static mrbc_debug_breakpoint *breakpoints = NULL;
static int breakpoint_num = 0;
static int breakpoints_max_len = 0;
static uint16_t next_breakpoint_id = 0;

static mrbc_irep_debug_info_file *get_file(mrbc_irep_debug_info *info, uint32_t pc) {
  mrbc_irep_debug_info_file **file = info->files;
  while(file < info->files + info->flen) {
    if(pc < (*file)->start_pos) break;
    file++;
  }
  --file;
  return *file;
}

static uint32_t mrbc_packed_int_decode(uint8_t *pack, int *len) {
  int idx = 0, offset = 0;
  uint32_t res = 0;

  while(offset < sizeof(uint32_t)) {
    res |= ((uint32_t)(pack[idx] & 0x7F)) << offset;
    if(!(pack[idx] & 0x80)) break;
    idx++;
    offset += 7;
  }
  
  *len = idx + 1;
  return res;
}

static uint16_t mrbc_debug_get_filename_idx(const mrbc_irep *irep, uint32_t pc) {
  const mrbc_irep_debug_info_file *file = get_file(irep->debug_info, pc);
  return file->filename_idx;
}

static int mrbc_debug_get_line(const mrbc_irep *irep, uint32_t pc) {
  if(irep == NULL || irep->debug_info == NULL) return -1;

  const mrbc_irep_debug_info_file *file = get_file(irep->debug_info, pc);
  if(file == NULL) return -1;

  switch (file->line_type)
  {
  case 0:
  case 1:
    return -1;
  case 2:
    uint8_t *pack = file->lines.packed_map;
    uint8_t *map_end = pack + file->line_entry_count;
    uint32_t current_pc = 0, line = 0, line_diff = 0;
    int len;
    while(pack < map_end) {
      current_pc += mrbc_packed_int_decode(pack, &len); pack += len;
      line_diff = mrbc_packed_int_decode(pack, &len); pack += len;
      if(pc < current_pc) break;
      line += line_diff;
    }
    return line;
  default:
    return -1;
  }
}

static int debug_read_line(char *buf, int max_len, int *len) {
  char c;
  *len = 0;
  if(!read_bytes) return 0;

  while(*len < max_len - 1 && read_bytes(&c, 1) > 0) {
    if(c == '\r' || c == '\n') return 1;
    buf[(*len)++] = c;
  }
  return 0;
}

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

static mrbc_debug_breakpoint *add_breakpoint(uint16_t filename_idx, uint16_t line) {
  if(breakpoint_num == breakpoints_max_len) {
    int new_max_len = breakpoints_max_len == 0 ? 8 : breakpoints_max_len * 2; // 初期は8, 以降は2倍
    mrbc_debug_breakpoint *new_braekpoints = (mrbc_debug_breakpoint*)mrbc_raw_realloc(breakpoints, new_max_len * sizeof(mrbc_debug_breakpoint));
    if(new_braekpoints == NULL) return NULL;
    breakpoints = new_braekpoints;
    breakpoints_max_len = new_max_len;
  }

  uint16_t id = next_breakpoint_id++;
  breakpoints[breakpoint_num] = (mrbc_debug_breakpoint){ filename_idx, line, id };
  return &breakpoints[breakpoint_num++];
}

static int delete_breakpoint(uint16_t id) {
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

static int delete_all_breakpoints() {
  if(breakpoints) mrbc_raw_free(breakpoints);
  breakpoints = NULL;
  breakpoint_num = 0;
  breakpoints_max_len = 0;
  return true;
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
  mrbc_hal_flush(1);
  return -1;
}

static int handle_command_break(mrbc_vm *vm, char *args, int len) {
  if(len == 0) { // `break`
    for(int i = 0; i < breakpoint_num; i++) {
      mrbc_printf("#%d %s:%d\n", breakpoints[i].id, vm->debug_filenames[breakpoints[i].filename_idx], breakpoints[i].line);
    }
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

  mrbc_debug_breakpoint *breakpoint = add_breakpoint((uint16_t)filename_idx, (uint16_t)line);
  if(breakpoint == NULL) return false;

  mrbc_printf("Breakpoint %d added at %s:%d\n", breakpoint->id, vm->debug_filenames[breakpoint->filename_idx], breakpoint->line);

  return true;
}

static int handle_command_delete(mrbc_vm *vm, char *args, int len) {
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

static int handle_command_info(mrbc_vm *vm, char *args, int len) {
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

static debug_resume_type handle_command(mrbc_vm *vm, char *command, int len) {
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
    handle_command_break(vm, command + 5, len - 5);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 1 && strncmp(command, "b", 1) == 0) {
    handle_command_break(vm, command + 1, len - 1);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 6 && strncmp(command, "delete", 6) == 0) {
    handle_command_delete(vm, command + 6, len - 6);
    return RESUME_TYPE_STAY;
  }
  else if(len >= 1 && strncmp(command, "d", 1) == 0) {
    handle_command_delete(vm, command + 1, len - 1);
    return RESUME_TYPE_STAY;
  }
  else if(len == 4 && strncmp(command, "info", 4) == 0) {
    handle_command_info(vm, command + 4, len - 4);
    return RESUME_TYPE_STAY;
  }

  return RESUME_TYPE_STAY;
}

static debug_resume_type handle_line(mrbc_vm *vm) {
  int total_len = 0;
  const int max_len = 256;
  char buf[256] = {0};

  while(true) {
    int len = 0;
    int res = debug_read_line(buf + total_len, max_len - total_len, &len);
    total_len += len;
    if(res) break;
    mrbc_hal_idle_cpu();
  }

  mrbc_printf("%d ", total_len);
  mrbc_nprint(buf, total_len);
  mrbc_print("\n");

  return handle_command(vm, buf, total_len);
}

void trace_callinfo(mrbc_vm *vm, mrbc_callinfo *callinfo, int depth) {
  if(callinfo == NULL) return;

  mrbc_irep *irep = callinfo->cur_irep;
  if(!irep->debug_info) return;

  uint32_t pc = callinfo->inst - irep->inst;
  int line = mrbc_debug_get_line(irep, pc);
  uint16_t filename_idx = mrbc_debug_get_filename_idx(irep, pc);

  mrbc_printf("#%d %s:%d\n", depth, vm->debug_filenames[filename_idx], line);
  mrbc_hal_flush(1);

  trace_callinfo(vm, callinfo->prev, depth + 1);
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

  int line = mrbc_debug_get_line(vm->cur_irep, pc);
  if(prev_line == line) return;

  trace_callinfo(vm, vm->callinfo_tail, 0);

  uint16_t filename_idx = mrbc_debug_get_filename_idx(irep, pc);
  int stop = false;
  if(prev_line == 0 || prev_resume_type == RESUME_TYPE_STEP) {
    stop = true;
    mrbc_printf("Stop: %s:%d\n", vm->debug_filenames[filename_idx], line);
  }

  if(prev_resume_type == RESUME_TYPE_CONTINUE) {
    for(int i = 0; !stop && i < breakpoint_num; i++) {
      if(filename_idx == breakpoints[i].filename_idx && line == breakpoints[i].line) {
        stop = true;
        mrbc_printf("Breakpoint %d: %s:%d\n", breakpoints[i].id, vm->debug_filenames[breakpoints[i].filename_idx], breakpoints[i].line);
      }
    }
  }

  if(stop) {
    prev_resume_type = RESUME_TYPE_STAY;
    while(prev_resume_type == RESUME_TYPE_STAY) {
      prev_resume_type = handle_line(vm);
    }
  }

  prev_line = line;
}

void debug_init(int (*read)(void *buf, int nbytes)) {
  read_bytes = read;
  mrbc_set_code_fetch_hook(debug_on_code_fetch);
}

#endif // defined(MRBC_USE_DEBUGGER)
