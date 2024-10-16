#include "einst.h"

#include "ieutil.h"

uint16_t einst_calc_size(const einst *inst)
{
  return sizeof(inst->mesh_id) + sizeof(inst->scale) + 4 * sizeof(*inst->rot) +
         sizeof(inst->trans);
}

uint8_t *einst_write(uint8_t *dst, const einst *i)
{
  dst = ie_write(dst, &i->mesh_id, sizeof(i->mesh_id)); // flags << 16 | mesh id
  dst = ie_write(dst, &i->scale, sizeof(i->scale));
  dst = ie_write(dst, &i->rot, 4 * sizeof(*i->rot));
  dst = ie_write(dst, &i->trans, sizeof(i->trans));
  return dst;
}
