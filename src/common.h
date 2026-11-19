#ifndef MRBC_DEBUG_SRC_COMMON_H_
#define MRBC_DEBUG_SRC_COMMON_H_

#include "vm.h"

mrbc_irep_debug_info_file *debug_find_file(const mrbc_irep_debug_info *info, uint32_t pc);

uint16_t debug_find_filename_idx(const mrbc_irep *irep, uint32_t pc);

int debug_find_line(const mrbc_irep *irep, uint32_t pc);

#endif