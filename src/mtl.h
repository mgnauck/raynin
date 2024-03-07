#ifndef MTL_H
#define MTL_H

typedef enum mtl_flags {
  MF_FLAT = 1, // Face normals
} mtl_flags;

typedef struct mtl {
  vec3    color;
  float   value;
} mtl;

#endif
