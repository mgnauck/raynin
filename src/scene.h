#ifndef SCENE_H
#define SCENE_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"

#define OBJECT_LINE_SIZE    4
#define SHAPE_LINE_SIZE     4
#define MATERIAL_LINE_SIZE  4

typedef enum shape_type {
  SPHERE = 1,
  BOX,
  CYLINDER,
  QUAD,
  MESH
} shape_type;

typedef enum material_type {
  LAMBERT = 1,
  METAL,
  GLASS
} material_type;

typedef struct object {
  shape_type shape_type;
  uint32_t shape_ofs;
  material_type mat_type;
  uint32_t mat_ofs;
} object;

typedef struct sphere {
  vec3 center;
  float radius;
} sphere;

typedef struct quad {
  vec3 q;
  float pad0;
  vec3 u;
  float pad1;
  vec3 v;
  float pad2;
} quad;

typedef struct lambert {
  vec3 albedo;
  float pad0;
} lambert;

typedef struct metal {
  vec3 albedo;
  float fuzz_radius;
} metal;

typedef struct glass {
  vec3 albedo;
  float refr_idx;
} glass;

typedef struct scene {
  uint32_t *object_buf;
  size_t object_line_count;
  float *shape_buf;
  size_t shape_line_count;
  float *material_buf;
  size_t material_line_count;
} scene;

object create_object(uint32_t shape_type, uint32_t shape_ofs, uint32_t mat_type, uint32_t mat_ofs);
sphere create_sphere(vec3 center, float radius);
quad create_quad(vec3 q, vec3 u, vec3 v);
lambert create_lambert(vec3 albedo);
metal create_metal(vec3 albedo, float fuzz_radius);
glass create_glass(vec3 albedo, float refr_idx);

scene *init_scene(size_t obj_line_capacity, size_t shape_line_capacity, size_t mat_line_capacity);
void release_scene(scene *scene);

uint32_t add_object(scene *scene, object obj); 
uint32_t add_sphere(scene *scene, sphere shape);
uint32_t add_quad(scene *scene, quad shape);
uint32_t add_lambert(scene *scene, lambert mat);
uint32_t add_metal(scene *scene, metal mat);
uint32_t add_glass(scene *scene, glass mat);

object *get_object(scene *scene, uint32_t line_index);
sphere *get_sphere(scene *scene, uint32_t line_index);
quad *get_quad(scene* scene, uint32_t line_index);
lambert *get_lambert(scene *scene, uint32_t line_index);
metal *get_metal(scene *scene, uint32_t line_index);
glass *get_glass(scene *scene, uint32_t line_index);

#endif
