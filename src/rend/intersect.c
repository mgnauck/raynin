#include "intersect.h"
#include "../acc/bvh.h"
#include "../scene/inst.h"
#include "../scene/mesh.h"
#include "../scene/tri.h"
#include "../scene/types.h"
#include "../sys/mutil.h"
#include "../sys/log.h"
#include "../util/ray.h"

// GPU efficient slabs test [Laine et al. 2013; Afra et al. 2016]
// https://www.jcgt.org/published/0007/03/04/paper-lowres.pdf
float intersect_aabb(const ray *r, float curr_t, vec3 min_ext, vec3 max_ext)
{
  vec3 t0 = vec3_mul(vec3_sub(min_ext, r->ori), r->inv_dir);
  vec3 t1 = vec3_mul(vec3_sub(max_ext, r->ori), r->inv_dir);

  float tnear = vec3_max_comp(vec3_min(t0, t1));
  float tfar = vec3_min_comp(vec3_max(t1, t0));

  return tnear <= tfar && tnear < curr_t && tfar > EPSILON ? tnear : MAX_DISTANCE;
}

void intersect_plane(const ray *r, uint32_t inst_id, hit *h)
{
  // Plane is infinite and at origin in XZ
  float d = r->dir.y;
  if(fabsf(d) > EPSILON) {
    float t = -r->ori.y / d;
    if(t < h->t && t > EPSILON) {
      h->t = t;
      h->e = (ST_PLANE << 16) | (inst_id & INST_ID_MASK);
    }
  }
}

void intersect_unitbox(const ray *r, uint32_t inst_id, hit *h)
{
  vec3 t0 = vec3_mul(vec3_sub((vec3){ -1.0f, -1.0f, -1.0f }, r->ori), r->inv_dir);
  vec3 t1 = vec3_mul(vec3_sub((vec3){  1.0f,  1.0f,  1.0f }, r->ori), r->inv_dir);

  float tnear = vec3_max_comp(vec3_min(t0, t1));
  float tfar = vec3_min_comp(vec3_max(t0, t1));

  if(tnear <= tfar) {
    if(tnear < h->t && tnear > EPSILON) {
      h->t = tnear;
      h->e = (ST_BOX << 16) | (inst_id & INST_ID_MASK);
      return;
    }
    if (tfar < h->t && tfar > EPSILON) {
      h->t = tfar;
      h->e = (ST_BOX << 16) | (inst_id & INST_ID_MASK);
    }
  }
}

void intersect_unitsphere(const ray *r, uint32_t inst_id, hit *h)
{
  float a = vec3_dot(r->dir, r->dir);
  float b = vec3_dot(r->ori, r->dir);
  float c = vec3_dot(r->ori, r->ori) - 1.0f;

  float d = b * b - a * c;
  if(d < 0)
    return;

  d = sqrtf(d);
  float t = (-b - d) / a;
  if(t <= EPSILON || h->t <= t) {
    t = (-b + d) / a;
    if(t <= EPSILON || h->t <= t)
      return;
  }

  h->t = t;
  h->e = (ST_SPHERE << 16) | (inst_id & INST_ID_MASK);
}

// Moeller/Trumbore ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
void intersect_tri(const ray *r, const tri *t, uint32_t inst_id, uint32_t tri_id, hit *h)
{
  // Vectors of two edges sharing vertex 0
  const vec3 edge1 = vec3_sub(t->v1, t->v0);
  const vec3 edge2 = vec3_sub(t->v2, t->v0);

  // Calculate determinant and u parameter later on
  const vec3 pvec = vec3_cross(r->dir, edge2);
  const float det = vec3_dot(edge1, pvec);

  if(fabsf(det) < EPSILON)
    // Ray in plane of triangle
    return;

  const float inv_det = 1.0f / det;

  // Distance vertex 0 to origin
  const vec3 tvec = vec3_sub(r->ori, t->v0);

  // Calculate parameter u and test bounds
  const float u = vec3_dot(tvec, pvec) * inv_det;
  if(u < 0.0 || u > 1.0)
    return;

  // Prepare to test for v
  const vec3 qvec = vec3_cross(tvec, edge1);

  // Calculate parameter u and test bounds
  const float v = vec3_dot(r->dir, qvec) * inv_det;
  if(v < 0.0f || u + v > 1.0f)
    return;

  // Calc distance
  const float dist = vec3_dot(edge2, qvec) * inv_det;
  if(dist > EPSILON && dist < h->t) {
    h->t = dist;
    h->u = u;
    h->v = v;
    // tri_id is relative to mesh (i.e. together with inst->data)
    h->e = (tri_id << 16) | (inst_id & INST_ID_MASK);
  }
}

void intersect_blas(const ray *r, const node *blas_nodes, const tri *tris, uint32_t inst_id, hit *h)
{
#define NODE_STACK_SIZE 32
  uint32_t    stack_pos = 0;
  const node  *node_stack[NODE_STACK_SIZE];
  const node  *n = blas_nodes;

  while(true) {
    if(n->children == 0) {
      // Leaf node, check triangle
      intersect_tri(r, &tris[n->idx], inst_id, n->idx, h);
      if(stack_pos > 0)
        n = node_stack[--stack_pos];
      else
        break;
    } else {
      // Interior node, check aabbs of children
      const node *c1 = &blas_nodes[n->children & 0xffff];
      const node *c2 = &blas_nodes[n->children >> 16];
      float d1 = intersect_aabb(r, h->t, c1->min, c1->max);
      float d2 = intersect_aabb(r, h->t, c2->min, c2->max);
      if(d1 > d2) {
        // Swap for nearer child
        float td = d1;
        d1 = d2;
        d2 = td;
        const node *tc = c1;
        c1 = c2;
        c2 = tc;
      }
      if(d1 == MAX_DISTANCE) {
        // Did miss both children, so check stack
        if(stack_pos > 0)
          n = node_stack[--stack_pos];
        else
          break;
      } else {
        // Continue with nearer child node
        n = c1;
        // Push farther child on stack if also a hit
        if(d2 < MAX_DISTANCE)
          node_stack[stack_pos++] = c2;
        if(stack_pos >= NODE_STACK_SIZE)
          logc("Exceeded node stack size!!");
      }
    }
  }
}

void intersect_inst(const ray *r, const inst *inst, const mesh *meshes, const node *blas_nodes, hit *h)
{
  ray r_obj;
  mat4 inv_transform;
  mat4_from_row3x4(inv_transform, inst->inv_transform);
  ray_transform(&r_obj, inv_transform, r);

  if(inst->data & SHAPE_TYPE_BIT) {
    // Intersect shape type
    switch((shape_type)(inst->data & MESH_SHAPE_MASK)) {
      case ST_PLANE:
        intersect_plane(&r_obj, inst->id, h);
        break;
      case ST_BOX:
        intersect_unitbox(&r_obj, inst->id, h);
        break;
      case ST_SPHERE:
        intersect_unitsphere(&r_obj, inst->id, h);
        break;
    }
  } else {
    // Intersect mesh type via its blas
    const mesh *mesh = &meshes[inst->data & MESH_SHAPE_MASK];
    const node *node = &blas_nodes[2 * mesh->ofs];
    intersect_blas(&r_obj, node, mesh->tris, inst->id, h);
  }
}

void intersect_tlas(const ray *r, const node *tlas_nodes, const inst *instances, const mesh *meshes, const node *blas_nodes, hit *h)
{
#define NODE_STACK_SIZE 32
  uint32_t    stack_pos = 0;
  const node  *node_stack[NODE_STACK_SIZE];
  const node  *n = tlas_nodes;

  while(true) {
    if(n->children == 0) {
      // Leaf node with a single instance assigned
      intersect_inst(r, &instances[n->idx], meshes, blas_nodes, h);
      if(stack_pos > 0)
        n = node_stack[--stack_pos];
      else
        break;
    } else {
      // Interior node, check aabbs of children
      const node *c1 = &tlas_nodes[n->children & 0xffff];
      const node *c2 = &tlas_nodes[n->children >> 16];
      float d1 = intersect_aabb(r, h->t, c1->min, c1->max);
      float d2 = intersect_aabb(r, h->t, c2->min, c2->max);
      if(d1 > d2) {
        // Swap for nearer child
        float td = d1;
        d1 = d2;
        d2 = td;
        const node *tc = c1;
        c1 = c2;
        c2 = tc;
      }
      if(d1 == MAX_DISTANCE) {
        // Did miss both children, so check stack
        if(stack_pos > 0)
          n = node_stack[--stack_pos];
        else
          break;
      } else {
        // Continue with nearer child node
        n = c1;
        // Push farther child on stack if also a hit
        if(d2 < MAX_DISTANCE)
          node_stack[stack_pos++] = c2;
        if(stack_pos >= NODE_STACK_SIZE)
          logc("Exceeded node stack size!!");
      }
    }
  }
}
