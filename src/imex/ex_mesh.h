#ifndef EX_MESH_H
#define EX_MESH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct vec3 vec3;

typedef struct ex_mesh {
  uint16_t  mtl_id; // 0xffff = not set, i.e. when mtl per triangle
  uint8_t   type; // Primitive type
  uint8_t   subx;
  uint8_t   suby;
  uint8_t   share_id;
  bool      face_nrms;
  bool      invert_nrms;
  bool      no_caps;
  float     in_radius; // TODO drop, not really used
  // TODO data for loaded meshes
} ex_mesh;

#endif
