#include "ray.h"
#include "mutil.h"
#include "view.h"
#include "cam.h"
#include "settings.h"

void ray_create(ray *ray, vec3 ori, vec3 dir)
{
  ray->ori = ori;
  ray->dir = dir;
  ray->inv_dir = (vec3){ 1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z };
}

void ray_create_primary(ray *ray, float x, float y, const view *v, const cam *c)
{
  // Viewplane pixel position
  vec3 pix_smpl = vec3_add(v->pix_top_left, vec3_add(
        vec3_scale(v->pix_delta_x, x), vec3_scale(v->pix_delta_y, y)));

#ifdef ANTIALIASING
  // Jitter viewplane position (AA)
  pix_smpl = vec3_add(pix_smpl, vec3_add(
        vec3_scale(v->pix_delta_x, pcg_randf() - 0.5f),
        vec3_scale(v->pix_delta_y, pcg_randf() - 0.5f)));
#endif

  // Jitter eye (DOF)
  vec3 eye_smpl = c->eye;
  if(c->foc_angle > 0.0f) {
    float foc_rad = c->foc_dist * tanf(0.5f * c->foc_angle * PI / 180.0f);
    vec3  disk_smpl = vec3_rand2_disk();
    eye_smpl = vec3_add(eye_smpl,
        vec3_scale(vec3_add(
            vec3_scale(c->right, disk_smpl.x),
            vec3_scale(c->up, disk_smpl.y)),
          foc_rad));
  }

  ray_create(ray, eye_smpl, vec3_unit(vec3_sub(pix_smpl, eye_smpl)));
}

void ray_transform(ray *dest, const mat4 m, const ray *r)
{
  dest->ori = mat4_mul_pos(m, r->ori);
  dest->dir = mat4_mul_dir(m, r->dir); // Intentionally do NOT normalize
  dest->inv_dir = (vec3){ 1.0f / dest->dir.x, 1.0f / dest->dir.y, 1.0f / dest->dir.z };
}
