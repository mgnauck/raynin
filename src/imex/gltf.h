#ifndef GLTF_H
#define GLTF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../util/vec3.h"

#define NAME_STR_LEN 256

typedef enum data_type {
  DT_SCALAR,
  DT_VEC3,
  DT_UNKNOWN // All the unsupported ones
} data_type;

typedef struct gltf_mtl {
  char name[NAME_STR_LEN];
  vec3 col;
  float metallic;
  float roughness;
  float ior;
  float refractive;
  vec3 emission;
} gltf_mtl;

typedef struct gltf_cam {
  char name[NAME_STR_LEN];
  float vert_fov;
  // Other properties not needed for now
} gltf_cam;

typedef struct gltf_prim {
  int32_t pos_idx;
  int32_t nrm_idx;
  int32_t ind_idx;
  int32_t mtl_idx;
} gltf_prim;

typedef struct gltf_mesh {
  char name[NAME_STR_LEN];
  uint32_t subx;    // Custom value subdivision
  uint32_t suby;    // Custom value subdivision
  float in_radius;  // Custom value (torus)
  bool no_caps;     // Custom value (cylinder)
  bool face_nrms;   // Custom value (generate face normals)
  bool invert_nrms; // Custom value (e.g. emitter skybox)
  uint8_t share_id; // Custom value (mesh shared between multiple scenes)
  gltf_prim *prims;
  uint32_t prim_cnt;
} gltf_mesh;

typedef struct gltf_accessor {
  int32_t bufview; // TODO: When undefined, the data MUST be initialized with
                   // zeros.
  uint32_t count;
  uint32_t byte_ofs; // Optional
  uint32_t comp_type;
  data_type data_type;
} gltf_accessor;

typedef struct gltf_bufview {
  uint32_t
      buf; // Only 1 supported, which is a .bin file that comes with the .gltf
  uint32_t byte_len;
  uint32_t byte_ofs; // Optional
  uint32_t byte_stride;
} gltf_bufview;

typedef struct gltf_node {
  char name[NAME_STR_LEN];
  int32_t mesh_idx;
  int32_t cam_idx;
  bool invisible; // Custom value (invisible)
  vec3 scale;
  float rot[4]; // Quaternion xyzw
  vec3 trans;
  // Not supporting node hierarchy currently
} gltf_node;

typedef struct gltf_data {
  gltf_mtl *mtls;
  uint32_t mtl_cnt;
  gltf_node *nodes;
  uint32_t node_cnt;
  uint32_t cam_node_cnt;
  gltf_mesh *meshes;
  uint32_t mesh_cnt;
  gltf_accessor *accessors;
  uint32_t accessor_cnt;
  gltf_bufview *bufviews;
  uint32_t bufview_cnt;
  gltf_cam *cams;
  uint32_t cam_cnt;
  vec3 bg_col; // Background color (custom value)
} gltf_data;

uint8_t gltf_read(gltf_data *data, const char *gltf, size_t gltf_sz);
void gltf_release(gltf_data *data);

#endif
