#ifndef MRBC_DEBUG_SRC_DEBUG_H_
#define MRBC_DEBUG_SRC_DEBUG_H_

typedef enum {
  RESUME_TYPE_STAY = 0,
  RESUME_TYPE_CONTINUE,
  RESUME_TYPE_STEP,
  RESUME_TYPE_NEXT,
  RESUME_TYPE_FINISH,
} debug_resume_type;

void debug_init(int (*read_bytes)(void *buf, int nbytes));

#endif
