#include "gltfimp.h"
#include "sutil.h"
#include "mutil.h"
#include "scene.h"
#include "mesh.h"
#include "bvh.h"
#include "mtl.h"
#include "tri.h"
#include "gltf.h"
#include "log.h"

uint8_t read_ubyte(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(uint8_t *)(buf + byte_ofs + sizeof(uint8_t) * i);
}

uint16_t read_ushort(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(uint16_t *)(buf + byte_ofs + sizeof(uint16_t) * i);
}

uint32_t read_uint(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(uint32_t *)(buf + byte_ofs + sizeof(uint32_t) * i);
}

float read_float(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(float *)(buf + byte_ofs + sizeof(float) * i);
}

vec3 read_vec(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return (vec3){
    .x = read_float(buf, byte_ofs, i * 3 + 0),
    .y = read_float(buf, byte_ofs, i * 3 + 1),
    .z = read_float(buf, byte_ofs, i * 3 + 2),
  };
}

void create_mesh(mesh *m, gltf_data *d, gltf_mesh *gm, const uint8_t *bin)
{
  // Calc triangle count from gltf mesh data
  for(uint32_t j=0; j<gm->prim_cnt; j++) {
    gltf_prim *p = &gm->prims[j];
    gltf_accessor *a = &d->accessors[p->ind_idx];
    if(a->data_type == DT_SCALAR)
      // 3 indices to vertices = 1 triangle
      m->tri_cnt += a->count / 3;
    else
      logc("Unexpected accessor data type found when creating a mesh. Ignoring primitive.");
  }

  mesh_init(m, m->tri_cnt);

  // Collect the data and copy to our actual mesh
  uint32_t tri_cnt = 0;
  for(uint32_t j=0; j<gm->prim_cnt; j++) {
    gltf_prim *p = &gm->prims[j];
    
    gltf_accessor *ind_acc = &d->accessors[p->ind_idx];
    if(ind_acc->data_type != DT_SCALAR) {
      logc("Ignoring primitive with non-scalar data type in index buffer");
      continue;
    }
    if(ind_acc->comp_type != 5121 && ind_acc->comp_type != 5123 && ind_acc->comp_type != 5125) {
      logc("Expected index buffer with component type of byte, short or int. Ignoring primitive.");
      continue;
    }

    gltf_accessor *pos_acc = &d->accessors[p->pos_idx];
    if(pos_acc->data_type != DT_VEC3) {
      logc("Ignoring primitive with non-VEC3 data type in position buffer.");
      continue;
    }
    if(pos_acc->comp_type != 5126) {
      logc("Expected position buffer with component type of float. Ignoring primitive.");
      continue;
    }

    gltf_accessor *nrm_acc = &d->accessors[p->nrm_idx];
    if(nrm_acc->data_type != DT_VEC3) {
      logc("Ignoring primitive with non-VEC3 data type in normal buffer.");
      continue;
    }
    if(nrm_acc->comp_type != 5126) {
      logc("Expected normal buffer with component type of float. Ignoring primitive.");
      continue;
    }
    
    gltf_bufview *ind_bv = &d->bufviews[ind_acc->bufview];
    gltf_bufview *pos_bv = &d->bufviews[pos_acc->bufview];
    gltf_bufview *nrm_bv = &d->bufviews[nrm_acc->bufview];

    for(uint32_t j=0; j<ind_acc->count / 3; j++) {

      tri *t = &m->tris[tri_cnt];
      uint32_t i = j * 3;

      uint32_t i0 = (ind_acc->comp_type == 5121) ?
        read_ubyte(bin, ind_bv->byte_ofs, i + 0) : (ind_acc->comp_type == 5123) ?
          read_ushort(bin, ind_bv->byte_ofs, i + 0) : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 0);

      uint32_t i1 = (ind_acc->comp_type == 5121) ?
        read_ubyte(bin, ind_bv->byte_ofs, i + 1) : (ind_acc->comp_type == 5123) ?
          read_ushort(bin, ind_bv->byte_ofs, i + 1) : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 1);

      uint32_t i2 = (ind_acc->comp_type == 5121) ?
        read_ubyte(bin, ind_bv->byte_ofs, i + 2) : (ind_acc->comp_type == 5123) ?
          read_ushort(bin, ind_bv->byte_ofs, i + 2) : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 2);

      //logc("Triangle %i with indices %i, %i, %i", j, i0, i1, i2);

      t->v0 = read_vec(bin, pos_bv->byte_ofs, i0);
      t->v1 = read_vec(bin, pos_bv->byte_ofs, i1);
      t->v2 = read_vec(bin, pos_bv->byte_ofs, i2);

      //vec3_logc("Tri with p0: ", t->v0);
      //vec3_logc("Tri with p1: ", t->v1);
      //vec3_logc("Tri with p2: ", t->v2);

      t->n0 = read_vec(bin, nrm_bv->byte_ofs, i0);
      t->n1 = read_vec(bin, nrm_bv->byte_ofs, i1);
      t->n2 = read_vec(bin, nrm_bv->byte_ofs, i2);

      //vec3_logc("Tri with n0: ", t->n0);
      //vec3_logc("Tri with n1: ", t->n1);
      //vec3_logc("Tri with n2: ", t->n2);

      m->centers[tri_cnt] = tri_calc_center(t);
      t->mtl = p->mtl_idx;
      tri_cnt++;
    }

    m->is_emissive |= mtl_is_emissive(&d->mtls[p->mtl_idx]);
  }

  logc("Created mesh with %i triangles", tri_cnt);

  // In case we skipped a primitive, adjust the tri cnt of the mesh
  if(m->tri_cnt != tri_cnt) {
    m->tri_cnt = tri_cnt;
    logc("#### WARN: Mesh is missing some triangle data, i.e. skipped gltf primitve.");
  }
}

uint8_t gltf_import(scene *s, const char *gltf, size_t gltf_sz, const uint8_t *bin, size_t bin_sz)
{
  // Parse the gltf
  gltf_data data = (gltf_data){ .nodes = NULL };
  if(gltf_read(&data, gltf, gltf_sz) != 0) {
    gltf_release(&data);
    return 1;
  }

  // Identify actual meshes
  uint32_t mesh_cnt = 0;
  for(uint32_t j=0; j<data.mesh_cnt; j++) {
    gltf_mesh *gm = &data.meshes[j];
    for(uint32_t i=0; i<gm->prim_cnt; i++) {
      gltf_prim *gp = &gm->prims[i]; 
      /*if(mtl_is_emissive(&s->mtls[gp->mtl_idx]) || gm->type == OT_MESH)*/ {
        gm->mesh_idx = mesh_cnt++;
        break;
      }
    }
  }

  // TODO
  // - Create instances
  // - Defer ltri allocation (remove from scene_init())
  // - mesh_finalize() into scene?
  // - Generator code separation
  // - Generated meshes, i.e. (gm->mesh_idx > 0 && gm->type != OT_MESH)
  // - Stride handling

  scene_init(s, mesh_cnt, data.mtl_cnt, data.node_cnt - data.cam_node_cnt, /* TODO ltri cnt */0);

  // Populate meshes with gltf mesh data
  for(uint32_t i=0; i<data.mesh_cnt; i++) {
    gltf_mesh *gm = &data.meshes[i];
    if(gm->mesh_idx >= 0) {
      mesh *m = &s->meshes[gm->mesh_idx];
      create_mesh(m, &data, gm, bin);
      mesh_finalize(s, m);
    }
  }

  gltf_release(&data);

  return 0;
}
