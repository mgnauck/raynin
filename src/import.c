#include "import.h"
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

void create_mesh_from_gltf(mesh *m, gltf_data *d, gltf_mesh *gm, const uint8_t *bin, bool *is_emissive)
{
  // Calc triangle count from gltf mesh data
  for(uint32_t j=0; j<gm->prim_cnt; j++) {
    gltf_prim *p = &gm->prims[j];
    gltf_accessor *a = &d->accessors[p->ind_idx];
    if(a->data_type == DT_SCALAR)
      // Assuming triangle (3 indices to vertices)
      m->tri_cnt += a->count / 3;
    else
      logc("Unexpected accessor data type found when creating a mesh. Ignoring primitive.");
  }

  mesh_init(m, m->tri_cnt);
  *is_emissive = false;

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

    *is_emissive |= mtl_is_emissive(&d->mtls[p->mtl_idx]);
  }

  logc("Created mesh from gltf with %i triangles", tri_cnt);

  // In case we skipped a primitive, adjust the tri cnt of the mesh
  if(m->tri_cnt != tri_cnt) {
    m->tri_cnt = tri_cnt;
    logc("#### WARN: Mesh is missing some triangle data, i.e. skipped gltf primitve.");
  }
}

void create_mesh(mesh *m, gltf_data *d, gltf_mesh *gm, bool *is_emissive)
{
  if(gm->type == OT_ICOSPHERE) {
    mesh_create_icosphere(m, gm->steps > 0 ? gm->steps : ICOSPHERE_DEFAULT_STEPS, gm->prims[0].mtl_idx, gm->face_nrms);
    logc("Created icosphere with %i triangles");
  } else if(gm->type == OT_SPHERE) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : SPHERE_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : SPHERE_DEFAULT_SUBY;
    mesh_create_uvsphere(m, 1.0f, subx, suby, gm->prims[0].mtl_idx, gm->face_nrms);
    logc("Created uvsphere with %i triangles", m->tri_cnt);
  } else if(gm->type == OT_CYLINDER) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : CYLINDER_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : CYLINDER_DEFAULT_SUBY;
    mesh_create_uvcylinder(m, 1.0f, 1.0f, subx, suby, gm->prims[0].mtl_idx, gm->face_nrms);
    logc("Created uvcylinder with %i triangles", m->tri_cnt);
  } else if(gm->type == OT_QUAD) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : QUAD_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : QUAD_DEFAULT_SUBY;
    mesh_create_quad(m, subx, suby, gm->prims[0].mtl_idx);
    logc("Created quad with %i triangles", m->tri_cnt);
  }
}

void process_mesh_node(scene *s, gltf_data *d, gltf_node *gn)
{
  gltf_mesh *gm = &d->meshes[gn->mesh_idx];
  
  // Final transformation
  mat4 scale, rot, trans, final;
  mat4_scale(scale, gn->scale);
  mat4_from_quat(rot, gn->rot[0], gn->rot[1], gn->rot[2], gn->rot[3]);
  mat4_trans(trans, gn->trans);
  mat4_mul(final, rot, scale);
  mat4_mul(final, trans, final);

  scene_add_mesh_inst(s, gm->mesh_idx, -1, final);
}

void process_cam_node(scene *s, gltf_data *d, gltf_node *gn)
{
  mat4 rot;
  mat4_from_quat(rot, gn->rot[0], gn->rot[1], gn->rot[2], gn->rot[3]);
  
  vec3 dir = vec3_unit(mat4_mul_dir(rot, (vec3){ 0.0f, 0.0f, -1.0f }));
  
  s->cam = (cam){ .vert_fov = d->cams[gn->cam_idx].vert_fov * 180.0f / PI, .foc_dist = 10.0f, .foc_angle = 0.0f };
  cam_set(&s->cam, gn->trans, vec3_add(gn->trans, dir));
}

uint8_t import_gltf(scene *s, const char *gltf, size_t gltf_sz, const uint8_t *bin, size_t bin_sz)
{
  // Parse the gltf
  gltf_data data = (gltf_data){ .nodes = NULL };
  if(gltf_read(&data, gltf, gltf_sz) != 0) {
    gltf_release(&data);
    logc("Failed to read gltf data");
    return 1;
  }

  // Identify actual meshes
  uint32_t mesh_cnt = 0;
  for(uint32_t j=0; j<data.mesh_cnt; j++) {
    gltf_mesh *gm = &data.meshes[j];

    if(gm->type == OT_UNKNOWN) {
      logc("#### WARN: Can not identify mesh %i because it has an unknown object type. Ignoring.", j);
      continue;
    }
    
    /// DEBUG
    gm->mesh_idx = mesh_cnt++;
    continue;

    // Antyhing that is a cylinder, plane/grid or is already a mesh in gltf or has multiple primitives
    // will also be a mesh in our renderer. We will generate mesh data for cylinders/planes but read the
    // data from the gltf in case it is an arbitrary mesh or has multiple primitives.
    if(gm->type == OT_MESH || gm->type == OT_CYLINDER || gm->type == OT_QUAD || gm->prim_cnt > 1) {
      gm->mesh_idx = mesh_cnt++;
      logc("gltf mesh %i will also be a mesh in the scene", j);
      continue;
    }

    // (Single) primitives with an emissive mtl will also be represented as mesh, no matter if we read
    // or generate the actual mesh data.
    for(uint32_t i=0; i<gm->prim_cnt; i++) {
      gltf_prim *gp = &gm->prims[i]; 
      if(mtl_is_emissive(&data.mtls[gp->mtl_idx])) {
        logc("gltf mesh %i will also be a mesh in the scene because its primitive %i is emissive", j, i);
        gm->mesh_idx = mesh_cnt++;
        break;
      }
    }

    // This leaves us with spheres and boxes which will be directly intersected as shape
  }

  // Allocate scene
  scene_init(s, mesh_cnt, data.mtl_cnt, data.node_cnt - data.cam_node_cnt);

  // Set default camera (might be overwritten if there is a cam in the scene)
  s->cam = (cam){ .vert_fov = 45.0f, .foc_dist = 10.0f, .foc_angle = 0.0f };
  cam_set(&s->cam, (vec3){ 0.0f, 5.0f, 10.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  // Convert gltf meshes to renderer meshes and attach to scene
  for(uint32_t i=0; i<data.mesh_cnt; i++) {
    gltf_mesh *gm = &data.meshes[i];
    
    if(gm->type == OT_UNKNOWN) {
      logc("#### WARN: Can not convert gltf mesh %i because it has an unknown object type. Ignoring.", i);
      continue;
    }

    if(gm->mesh_idx >= 0) {
      bool is_emissive;

      // Acquire an empty mesh from the scene
      mesh *m = scene_acquire_mesh(s);
      
      if(gm->type == OT_MESH || gm->type == OT_BOX /* TODO add box gen */ || gm->prim_cnt > 1) {
        // Read mesh data from gltf and create a renderer mesh
        create_mesh_from_gltf(m, &data, gm, bin, &is_emissive);
      } else {
        // Generate the mesh data for the renderer (ico/spheres, cylinders, quads)
        create_mesh(m, &data, gm, &is_emissive);
      }

      // Attach the populated mesh to the scene
      scene_attach_mesh(s, m, is_emissive);
    }
  }

  // Add materials to scene
  for(uint32_t i=0; i<data.mtl_cnt; i++)
    scene_add_mtl(s, &data.mtls[i]);

  // Add nodes as scene instances
  for(uint32_t i=0; i<data.node_cnt; i++) {
    gltf_node *gn = &data.nodes[i];
    if(gn->mesh_idx >= 0)
      process_mesh_node(s, &data, gn);
    else if(gn->cam_idx >= 0)
      // Last camera wins for now (our scene does not support multiple cams)
      process_cam_node(s, &data, gn);
    else
      logc("Unexpected/unknown node found. Ignoring.");
  }

  // Finalize the scene data (bvhs and ltris)
  scene_finalize(s);
  
  gltf_release(&data);

  logc("Create scene with %i meshes, %i materials and %i instances", s->mesh_cnt, s->mtl_cnt, s->inst_cnt);

  return 0;
}

void import_mesh(mesh *m, const uint8_t *data, uint32_t mtl, bool faceNormals)
{
  uint32_t ofs = 0;

  uint32_t vertex_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);

  uint32_t normal_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(normal_cnt);

  uint32_t uv_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(uv_cnt);

  uint32_t tri_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(tri_cnt);

  float *vertices = (float *)(data + ofs);
  ofs += vertex_cnt * 3 * sizeof(*vertices);

  float *normals = (float *)(data + ofs);
  ofs += normal_cnt * 3 * sizeof(*normals);

  float *uvs = (float *)(data + ofs);
  ofs += uv_cnt * 2 * sizeof(*uvs);

  uint32_t *indices = (uint32_t *)(data + ofs);

  mesh_init(m, tri_cnt);

  uint32_t items = (1 + (uv_cnt > 0 ? 1 : 0) + (normal_cnt > 0 ? 1 : 0));

  for(uint32_t i=0; i<tri_cnt; i++) {
    tri *t = &m->tris[i];
    uint32_t index = 3 * items * i;

    memcpy(&t->v0, &vertices[3 * indices[index]], sizeof(t->v0));
    memcpy(&t->v1, &vertices[3 * indices[index + items]], sizeof(t->v1));
    memcpy(&t->v2, &vertices[3 * indices[index + items + items]], sizeof(t->v2));
#ifdef TEXTURE_SUPPORT // Untested
    if(uv_cnt > 0 ) {
      memcpy(t->uv0, &uvs[2 * indices[index + 1]], 2 * sizeof(*t->uv0));
      memcpy(t->uv1, &uvs[2 * indices[index + items + 1]], 2 * sizeof(*t->uv1));
      memcpy(t->uv2, &uvs[2 * indices[index + items + items + 1]], 2 * sizeof(*t->uv2));
    }
#endif
    if(!faceNormals && normal_cnt > 0) {
      memcpy(&t->n0, &normals[3 * indices[index + 2]], sizeof(t->n0));
      memcpy(&t->n1, &normals[3 * indices[index + items + 2]], sizeof(t->n1));
      memcpy(&t->n2, &normals[3 * indices[index + items + items + 2]], sizeof(t->n2));
    } else 
      t->n0 = t->n1 = t->n2 = vec3_unit(vec3_cross(vec3_sub(t->v0, t->v1), vec3_sub(t->v2, t->v1)));

    t->mtl = mtl;
    m->centers[i] = tri_calc_center(t);
  }
}
