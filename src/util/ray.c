#include "ray.h"
#include "../settings.h"
#include "../sys/mutil.h"

void ray_create(ray *ray, vec3 ori, vec3 dir)
{
  ray->ori = ori;
  ray->dir = dir;
  ray->inv_dir = (vec3){ 1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z };
}

void ray_transform(ray *dest, const mat4 m, const ray *r)
{
  dest->ori = mat4_mul_pos(m, r->ori);
  dest->dir = mat4_mul_dir(m, r->dir); // Intentionally do NOT normalize
  dest->inv_dir = (vec3){ 1.0f / dest->dir.x, 1.0f / dest->dir.y, 1.0f / dest->dir.z };
}
