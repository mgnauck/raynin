#include "cam.h"
#include "../sys/mutil.h"
#include "../util/ray.h"
#include "view.h"

void cam_calc_base(cam *c)
{
  c->right = vec3_cross((vec3){ 0.0f, 1.0f, 0.0f }, c->fwd);
  c->up = vec3_cross(c->fwd, c->right);
}

void cam_set(cam *c, vec3 look_from, vec3 look_at)
{
  c->eye = look_from;
  c->fwd = vec3_unit(vec3_sub(look_from, look_at));
  
  cam_calc_base(c);
}

void cam_set_dir(cam *c, vec3 dir)
{
  c->fwd = dir;

  cam_calc_base(c);
}

void cam_create_primary_ray(ray *ray, float x, float y, const view *v, const cam *c)
{
  // Viewplane pixel position
  vec3 pix_smpl = vec3_add(v->pix_top_left, vec3_add(
        vec3_scale(v->pix_delta_x, x), vec3_scale(v->pix_delta_y, y)));

  // Jitter viewplane position (AA)
  pix_smpl = vec3_add(pix_smpl, vec3_add(
        vec3_scale(v->pix_delta_x, pcg_randf() - 0.5f),
        vec3_scale(v->pix_delta_y, pcg_randf() - 0.5f)));

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
