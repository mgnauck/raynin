#ifndef GUTIL_H
#define GUTIL_H

#include <stddef.h>

#define GLOB_BUF_OFS_CFG    0
#define GLOB_BUF_OFS_FRAME  16
#define GLOB_BUF_OFS_CAM    48
#define GLOB_BUF_OFS_VIEW   96

#define GLOB_BUF_SIZE       144

typedef enum buf_type {
  GLOB = 0,
  TRI,
  TRI_DATA,
  INDEX,
  BVH_NODE,
  TLAS_NODE,
  INST,
  MAT
} buf_type;

extern void gpu_create_res(size_t glob_sz, size_t tri_sz, size_t tri_data_sz,
    size_t index_sz, size_t bvh_node_sz, size_t tlas_node_sz, size_t inst_sz,
    size_t mat_sz);

extern void gpu_write_buf(buf_type src_type, size_t dest_ofs,
    const void *src, size_t size);

#endif
