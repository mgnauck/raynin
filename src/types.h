#ifndef TYPES_H
#define TYPES_H

typedef enum res_type {
  RT_CAM_VIEW  = 1,
  RT_MESH      = 2,
  RT_MTL       = 4,
  RT_INST      = 8
} res_type;

typedef enum shape_type {
  ST_SPHERE   = 0,
  ST_CYLINDER,
  ST_BOX
} shape_type;

#endif
