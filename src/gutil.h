#ifndef GUTIL_H
#define GUTIL_H

#include <stddef.h>

typedef enum buf_type {
  GLOB,
  BVH,
  OBJ,
  SHAPE,
  MAT
} buf_type;

extern void gpu_create_res(size_t glob_sz, size_t bvh_sz, size_t obj_sz,
    size_t shape_sz, size_t mat_sz);

extern void gpu_write_buf(buf_type src_type, size_t dest_ofs,
    const void *src, size_t size);

#endif
