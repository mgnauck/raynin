#include "export.h"
#include "ecam.h"
#include "einst.h"
#include "emesh.h"
#include "escene.h"
#include "../scene/mtl.h"
#include "../sys/log.h"
#include "../sys/sutil.h"

uint8_t export_bin(escene *scenes, uint8_t scene_cnt, uint8_t **bin, size_t *bin_sz)
{
  // Account for offset to where scene data starts + the number of scenes
  uint32_t sz = 4 + 1;

  for(uint8_t i=0; i<scene_cnt; i++) {
    sz += escene_calc_size(&scenes[i]);
  }

  // TODO

  return 0;
}
