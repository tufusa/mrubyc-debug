#include <stddef.h>

#include "common.h"

mrbc_irep_debug_info_file *debug_find_file(const mrbc_irep_debug_info * info, uint32_t pc) {
  mrbc_irep_debug_info_file **file = info->files;
  while(file < info->files + info->flen) {
    if(pc < (*file)->start_pos) break;
    file++;
  }
  --file;
  return *file;
}

uint16_t debug_find_filename_idx(const mrbc_irep *irep, uint32_t pc) {
  const mrbc_irep_debug_info_file *file = debug_find_file(irep->debug_info, pc);
  return file->filename_idx;
}

static uint32_t decode_mrbc_packed_int(const uint8_t *pack, int *len) {
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

int debug_find_line(const mrbc_irep *irep, uint32_t pc) {
  if(irep == NULL || irep->debug_info == NULL) return -1;

  const mrbc_irep_debug_info_file *file = debug_find_file(irep->debug_info, pc);
  if(file == NULL) return -1;

  switch (file->line_type)
  {
  case 0:
  case 1:
    return -1;
  case 2:
    uint8_t *pack = (uint8_t*)file->lines.packed_map;
    uint8_t *map_end = pack + file->line_entry_count;
    uint32_t current_pc = 0, line = 0, line_diff = 0;
    int len;
    while(pack < map_end) {
      current_pc += decode_mrbc_packed_int(pack, &len); pack += len;
      line_diff = decode_mrbc_packed_int(pack, &len); pack += len;
      if(pc < current_pc) break;
      line += line_diff;
    }
    return line;
  default:
    return -1;
  }
}
