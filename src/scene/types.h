#ifndef TYPES_H
#define TYPES_H

typedef enum res_type {
  RT_CAM_VIEW = 1,
  RT_MESH     = 2,
  RT_TRI      = 4,
  RT_LTRI     = 8,
  RT_MTL      = 16,
  RT_INST     = 32
} res_type;

typedef enum shape_type {
  ST_PLANE    = 0,
  ST_BOX      = 1,
  ST_SPHERE   = 2,
} shape_type;

#endif
