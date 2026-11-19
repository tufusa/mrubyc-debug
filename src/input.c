#include <stddef.h>
#include <stdbool.h>

#include "hal.h"

#include "input.h"

static int (*read_bytes)(void *buf, int nbytes) = NULL;

void set_read_bytes(int (*read)(void *buf, int nbytes)) {
  read_bytes = read;
}

static bool try_read_line(char *buf, int max_len, int *len) {
  char c;
  *len = 0;
  if(!read_bytes) return false;

  while(*len < max_len - 1 && read_bytes(&c, 1) > 0) {
    if(c == '\r' || c == '\n') return true;
    buf[(*len)++] = c;
  }
  return false;
}

int read_line(char *buf, int max_len) {
  int total_len = 0;

  while(total_len < max_len) {
    int len = 0;
    bool completed = try_read_line(buf + total_len, max_len - total_len, &len);
    total_len += len;
    if(completed) break;
    mrbc_hal_idle_cpu();
  }

  return total_len;
}
