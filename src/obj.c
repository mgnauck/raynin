#include "obj.h"

obj obj_create(uint32_t shape_type, uint32_t shape_idx, uint32_t mat_type, uint32_t mat_idx)
{
  return (obj){
    .shape_type = shape_type,
    .shape_idx = shape_idx,
    .mat_type = mat_type,
    .mat_idx = mat_idx };
}
