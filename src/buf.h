#ifndef BUF_H
#define BUF_H

#include <stdint.h>
#include "gutil.h"

void      buf_init(uint32_t buf_cnt);
void      buf_reserve(buf_type type, uint32_t buf_type_size, uint32_t elem_cnt);
void      *buf_acquire(buf_type type, uint32_t elem_cnt);
void      *buf_ptr(buf_type type, uint32_t idx);
uint32_t  buf_idx(buf_type type, const void *ptr);
uint32_t  buf_len(buf_type type);
void      buf_release();

#endif
