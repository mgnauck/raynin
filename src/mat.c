#include "mat.h"
#include "mutil.h"

void mat_rand(mat *m)
{
  m->color = vec3_rand();
  m->value = pcg_randf();
}
