#ifndef BUF_H
#define BUF_H

#include <stddef.h>
#include "gutil.h"

void    buf_init(size_t buf_cnt);
void    buf_reserve(buf_type type, size_t buf_type_size, size_t elem_cnt);
void    *buf_acquire(buf_type type, size_t elem_cnt);
void    *buf_ptr(buf_type type, size_t idx);
size_t  buf_idx(buf_type type, const void *ptr);
size_t  buf_len(buf_type type);
void    buf_release();

#endif
