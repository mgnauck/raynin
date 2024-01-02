#include "obj.h"

obj obj_create(shape_type st, uint32_t shape_idx, mat_type mt, uint32_t mat_idx)
{
  return (obj){ .shape_type = st, .shape_idx = shape_idx,
    .mat_type = mt, .mat_idx = mat_idx };
}
