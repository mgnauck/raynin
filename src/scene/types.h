#ifndef TYPES_H
#define TYPES_H

typedef enum res_type {
  RT_CFG = 1,
  RT_CAM = 2,
  RT_MTL = 4,
  RT_TRI = 8,
  RT_LTRI = 16,
  RT_INST = 32,
  RT_BLAS = 64,
} res_type;

#endif
