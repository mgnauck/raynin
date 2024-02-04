#include "buf.h"
#include "sutil.h"

struct buf {
  char    *ptr; // base pointer in bytes
  size_t  tsz;  // sizeof(buffer type)
  size_t  idx;  // index in buffer type (not bytes)
} *buf;

size_t total_buf_cnt = 0;

void buf_init(size_t buf_cnt)
{
  total_buf_cnt = buf_cnt;

  buf = malloc(total_buf_cnt * sizeof(*buf));

  for(size_t i=0; i<total_buf_cnt; i++)
    buf[i].ptr = NULL;
}

void buf_reserve(buf_type type, size_t buf_type_sz, size_t elem_cnt)
{
  buf[type].ptr = malloc(elem_cnt * buf_type_sz);
  buf[type].tsz = buf_type_sz;
  buf[type].idx = 0;
}

void *buf_acquire(buf_type type, size_t elem_cnt)
{
  size_t curr_idx = buf[type].idx;
  buf[type].idx += elem_cnt;
  return buf[type].ptr + curr_idx * buf[type].tsz;
}

void *buf_ptr(buf_type type, size_t idx)
{
  return buf[type].ptr + idx * buf[type].tsz;
}

size_t buf_idx(buf_type type, const void *ptr)
{
  return ((char *)ptr - buf[type].ptr) / buf[type].tsz;
}

size_t buf_len(buf_type type)
{
  return buf[type].idx * buf[type].tsz;
}

void buf_release()
{
  for(size_t i=0; i<total_buf_cnt; i++)
    free(buf[i].ptr);
  free(buf);
  total_buf_cnt = 0;
}
