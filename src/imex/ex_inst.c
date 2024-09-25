#include "ex_inst.h"
#include "ieutil.h"

uint16_t ex_inst_calc_size(ex_inst const *inst)
{
  return sizeof(inst->mesh_id) + sizeof(inst->scale) +
    4 * sizeof(*inst->rot) + sizeof(inst->trans);
}

uint8_t *ex_inst_write(ex_inst const *i, uint8_t *dst)
{
  dst = ie_write(dst, &i->mesh_id, sizeof(i->mesh_id));
  dst = ie_write(dst, &i->scale, sizeof(i->scale));
  dst = ie_write(dst, &i->rot, 4 * sizeof(*i->rot));
  dst = ie_write(dst, &i->trans, sizeof(i->trans));
  return dst;
}
