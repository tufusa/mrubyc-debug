#ifndef MRBC_DEBUG_SRC_INPUT_H_
#define MRBC_DEBUG_SRC_INPUT_H_

void set_read_bytes(int (*read_bytes)(void *buf, int nbytes));

int read_line(char *buf, int max_len);

#endif
