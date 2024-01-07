#ifndef OBJ_H
#define OBJ_H

#include <stdint.h>

typedef enum shape_type {
  SPHERE = 1,
  BOX,
  CYLINDER,
  QUAD,
  MESH
} shape_type;

typedef enum mat_type {
  LAMBERT = 1,
  METAL,
  GLASS,
  EMITTER
} mat_type;

typedef struct obj {
  shape_type  shape_type;
  uint32_t    shape_idx;
  mat_type    mat_type;
  uint32_t    mat_idx;
} obj;

#endif
