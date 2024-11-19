#include "import.h"
#include "../base/log.h"
#include "../base/math.h"
#include "../base/stdlib.h"
#include "../base/string.h"
#include "../scene/cam.h"
#include "../scene/inst.h"
#include "../scene/mesh.h"
#include "../scene/mtl.h"
#include "../scene/scene.h"
#include "../scene/tri.h"
#include "gltf.h"

// Make silent
#undef logc
#define logc(...) ((void)0)

typedef struct mesh_ref {
  int32_t   mesh_idx;     // Render mesh index
  bool      is_emissive;  // True if mesh has emissive mtl
  uint32_t  inst_cnt;     // Instances pointing to a mesh
} mesh_ref;

typedef enum obj_type {
  OT_QUAD = 0,
  OT_BOX,
  OT_SPHERE,
  OT_CYLINDER,
  OT_TORUS,
  OT_MESH,
} obj_type;

obj_type get_mesh_type(const char *name)
{
  if(strstr_lower(name, "grid") || strstr_lower(name, "plane") ||
     strstr_lower(name, "quad"))
    return OT_QUAD;
  else if(strstr_lower(name, "cube") || strstr_lower(name, "box"))
    return OT_BOX;
  else if(strstr_lower(name, "sphere") || strstr_lower(name, "icosphere"))
    return OT_SPHERE;
  else if(strstr_lower(name, "cylinder"))
    return OT_CYLINDER;
  else if(strstr_lower(name, "torus") || strstr_lower(name, "ring") ||
          strstr_lower(name, "donut"))
    return OT_TORUS;
  return OT_MESH; // Everything else
}

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

void load_mesh_data(mesh *m, gltf_data *d, gltf_mesh *gm, const uint8_t *bin)
{
  // Calc triangle count from gltf mesh data
  for(uint32_t j = 0; j < gm->prim_cnt; j++) {
    gltf_prim *p = &gm->prims[j];
    gltf_accessor *a = &d->accessors[p->ind_idx];
    if(a->data_type == DT_SCALAR)
      // Assuming triangle (3 indices to vertices)
      m->tri_cnt += a->count / 3;
    else
      logc("Unexpected accessor data type found when creating a mesh. Ignoring "
           "primitive.");
  }

  mesh_init(m, m->tri_cnt);

  float inv = gm->invert_nrms ? -1.0f : 1.0f;

  // Collect the data and copy to our actual mesh
  uint32_t tri_cnt = 0;
  for(uint32_t j = 0; j < gm->prim_cnt; j++) {
    gltf_prim *p = &gm->prims[j];

    gltf_accessor *ind_acc = &d->accessors[p->ind_idx];
    if(ind_acc->data_type != DT_SCALAR) {
      logc("Ignoring primitive with non-scalar data type in index buffer");
      continue;
    }
    if(ind_acc->comp_type != 5121 && ind_acc->comp_type != 5123 &&
       ind_acc->comp_type != 5125) {
      logc("Expected index buffer with component type of byte, short or int. "
           "Ignoring primitive.");
      continue;
    }

    gltf_accessor *pos_acc = &d->accessors[p->pos_idx];
    if(pos_acc->data_type != DT_VEC3) {
      logc("Ignoring primitive with non-VEC3 data type in position buffer.");
      continue;
    }
    if(pos_acc->comp_type != 5126) {
      logc("Expected position buffer with component type of float. Ignoring "
           "primitive.");
      continue;
    }

    gltf_accessor *nrm_acc = &d->accessors[p->nrm_idx];
    if(nrm_acc->data_type != DT_VEC3) {
      logc("Ignoring primitive with non-VEC3 data type in normal buffer.");
      continue;
    }
    if(nrm_acc->comp_type != 5126) {
      logc("Expected normal buffer with component type of float. Ignoring "
           "primitive.");
      continue;
    }

    gltf_bufview *ind_bv = &d->bufviews[ind_acc->bufview];
    gltf_bufview *pos_bv = &d->bufviews[pos_acc->bufview];
    gltf_bufview *nrm_bv = &d->bufviews[nrm_acc->bufview];

    for(uint32_t j = 0; j < ind_acc->count / 3; j++) {
      tri *t = &m->tris[tri_cnt];
      tri_nrm *tn = &m->tri_nrms[tri_cnt];
      uint32_t i = j * 3;

      uint32_t i0 = (ind_acc->comp_type == 5121)
                        ? read_ubyte(bin, ind_bv->byte_ofs, i + 0)
                    : (ind_acc->comp_type == 5123)
                        ? read_ushort(bin, ind_bv->byte_ofs, i + 0)
                        : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 0);

      uint32_t i1 = (ind_acc->comp_type == 5121)
                        ? read_ubyte(bin, ind_bv->byte_ofs, i + 1)
                    : (ind_acc->comp_type == 5123)
                        ? read_ushort(bin, ind_bv->byte_ofs, i + 1)
                        : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 1);

      uint32_t i2 = (ind_acc->comp_type == 5121)
                        ? read_ubyte(bin, ind_bv->byte_ofs, i + 2)
                    : (ind_acc->comp_type == 5123)
                        ? read_ushort(bin, ind_bv->byte_ofs, i + 2)
                        : /* 5125 */ read_uint(bin, ind_bv->byte_ofs, i + 2);

      t->v0 = read_vec(bin, pos_bv->byte_ofs, i0);
      t->v1 = read_vec(bin, pos_bv->byte_ofs, i1);
      t->v2 = read_vec(bin, pos_bv->byte_ofs, i2);

      tn->n0 = vec3_scale(read_vec(bin, nrm_bv->byte_ofs, i0), inv);
      tn->n1 = vec3_scale(read_vec(bin, nrm_bv->byte_ofs, i1), inv);
      tn->n2 = vec3_scale(read_vec(bin, nrm_bv->byte_ofs, i2), inv);

      tn->mtl = p->mtl_idx;
      tri_cnt++;
    }
  }

  // In case we skipped a primitive, adjust the tri cnt of the mesh
  if(m->tri_cnt != tri_cnt) {
    m->tri_cnt = tri_cnt;
    logc("#### WARN: Mesh is missing some triangle data, i.e. skipped gltf "
         "primitve.");
  }

  logc("Loaded mesh from gltf data with %i triangles.", m->tri_cnt);
}

void generate_mesh_data(mesh *m, gltf_mesh *gm, bool is_emissive)
{
  obj_type type = get_mesh_type(gm->name);
  if(type == OT_TORUS) {
    uint32_t subx = gm->subx > 0 ? gm->subx : TORUS_DEFAULT_SUB_INNER;
    uint32_t suby = gm->suby > 0 ? gm->suby : TORUS_DEFAULT_SUB_OUTER;
    float in_radius =
        gm->in_radius > 0.0 ? gm->in_radius : TORUS_DEFAULT_INNER_RADIUS;
    mesh_create_torus(m, in_radius, TORUS_DEFAULT_OUTER_RADIUS, subx, suby,
                      gm->prims[0].mtl_idx, gm->face_nrms | is_emissive,
                      gm->invert_nrms);
    logc("Generated torus with %i triangles.", m->tri_cnt);
  } else if(type == OT_SPHERE) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : SPHERE_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : SPHERE_DEFAULT_SUBY;
    mesh_create_sphere(m, 1.0f, subx, suby, gm->prims[0].mtl_idx,
                       gm->face_nrms | is_emissive, gm->invert_nrms);
    logc("Generated uvsphere with %i triangles.", m->tri_cnt);
  } else if(type == OT_CYLINDER) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : CYLINDER_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : CYLINDER_DEFAULT_SUBY;
    mesh_create_cylinder(m, 1.0f, 2.0f, subx, suby, !gm->no_caps,
                         gm->prims[0].mtl_idx, gm->face_nrms | is_emissive,
                         gm->invert_nrms);
    logc("Generated uvcylinder with %i triangles.", m->tri_cnt);
  } else if(type == OT_BOX) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : BOX_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : BOX_DEFAULT_SUBY;
    mesh_create_box(m, subx, suby, gm->prims[0].mtl_idx, gm->invert_nrms);
    logc("Generated box with %i triangles.", m->tri_cnt);
  } else if(type == OT_QUAD) {
    uint32_t subx = (gm->subx > 0) ? gm->subx : QUAD_DEFAULT_SUBX;
    uint32_t suby = (gm->suby > 0) ? gm->suby : QUAD_DEFAULT_SUBY;
    mesh_create_quad(m, subx, suby, gm->prims[0].mtl_idx, gm->invert_nrms);
    logc("Generated quad with %i triangles.", m->tri_cnt);
  }
}

void process_mesh_node(scene *s, gltf_data *d, gltf_node *gn, uint32_t node_idx,
                       int32_t *render_mesh_ids)
{
  // Get node transformation
  mat4 scale, rot, trans, final;
  mat4_scale(scale, gn->scale);
  mat4_from_quat(rot, gn->rot[0], gn->rot[1], gn->rot[2], gn->rot[3]);
  mat4_trans(trans, gn->trans);
  mat4_mul(final, rot, scale);
  mat4_mul(final, trans, final);

  // Get mesh data
  gltf_mesh *gm = &d->meshes[gn->mesh_idx];
  int32_t mesh_id = render_mesh_ids[node_idx];

  // Create instance
  if(mesh_id >= 0) {
    // Mesh instance (mesh was either loaded or generated)
    uint32_t flags = gn->invisible > 0 ? IF_INVISIBLE : 0;
    uint16_t inst_id =
        scene_add_inst(s, mesh_id, NO_MTL_OVERRIDE, flags, final);

    logc("Created mesh instance of gltf mesh %i (%s) (render mesh %i) for node "
         "%i (%s).",
         gn->mesh_idx, gm->name, mesh_id, node_idx, gn->name);
  } else {
    logc("#### WARN: Node %i (%s) points at gltf mesh %i (%s) with invalid "
         "data. No instance will be created.",
         node_idx, gn->name, gn->mesh_idx, gm->name);
  }
}

void process_cam_node(scene *s, gltf_data *d, gltf_node *gn)
{
  mat4 rot;
  mat4_from_quat(rot, gn->rot[0], gn->rot[1], gn->rot[2], gn->rot[3]);

  vec3 dir = vec3_unit(mat4_mul_dir(rot, (vec3){0.0f, 0.0f, 1.0f}));

  gltf_cam *gc = &d->cams[gn->cam_idx];
  cam *c = scene_get_cam(s, gn->cam_idx);
  *c = (cam){.vert_fov = gc->vert_fov * 180.0f / PI,
             .foc_dist = 10.0f,
             .foc_angle = 0.0f};
  cam_set(c, gn->trans, dir);

  logc("Created camera %i from node (%s) pointing to gltf cam %i (%s).",
       gn->cam_idx, gn->name, gn->cam_idx, gc->name);
}

bool check_is_emissive_mtl(gltf_mtl *gm)
{
  return vec3_max_comp(gm->emission) > 0.0f;
}

void create_mtl(mtl *m, gltf_mtl *gm)
{
  *m = (mtl){.metallic = gm->metallic,
             .roughness = gm->roughness,
             .ior = gm->ior,
             .refractive = gm->refractive > 0.99f ? 1.0f : 0.0f};
  bool is_emissive = check_is_emissive_mtl(gm);
  m->col = is_emissive ? gm->emission : gm->col;
  m->emissive = is_emissive ? 1.0f : 0.0f;
}

bool check_is_emissive_mesh(gltf_mesh *gm, gltf_data *d)
{
  for(uint32_t i = 0; i < gm->prim_cnt; i++)
    if(check_is_emissive_mtl(&d->mtls[gm->prims[i].mtl_idx]))
      return true;
  return false;
}

bool check_has_valid_mtl(gltf_mesh *gm, gltf_data *d)
{
  for(uint32_t i = 0; i < gm->prim_cnt; i++)
    if(gm->prims[i].mtl_idx == -1)
      return false;
  return true;
}

uint8_t import_gltf(scene *s, const char *gltf, size_t gltf_sz,
                    const uint8_t *bin, size_t bin_sz)
{
  // Parse the gltf
  logc("-------- Reading gltf:");
  gltf_data data = (gltf_data){.nodes = NULL};
  if(gltf_read(&data, gltf, gltf_sz) != 0) {
    gltf_release(&data);
    logc("Failed to read gltf data.");
    return 1;
  }

  // Mapping gltf meshes to renderer meshes
  mesh_ref *mesh_map = malloc(data.mesh_cnt * sizeof(*mesh_map));
  uint32_t mesh_cnt = 0;

  // Identify actual meshes
  logc("-------- Identifying meshes:");
  for(uint32_t j = 0; j < data.mesh_cnt; j++) {
    gltf_mesh *gm = &data.meshes[j];
    mesh_ref *mr = &mesh_map[j];
    if(check_has_valid_mtl(gm, &data)) {
      mr->inst_cnt = 0; // Init for duplicate identification
      bool is_emissive = check_is_emissive_mesh(gm, &data);
      mr->mesh_idx = 1; // Assign something that is not -1 to indicate it is a
                        // mesh, actual index will follow during mesh creation
      mr->is_emissive = is_emissive;
      mesh_cnt++;
      logc("Gltf mesh %i (%s) will be a render mesh in the scene. This mesh is "
           "%semissive.",
           j, gm->name, is_emissive ? "" : "not ");
    } else {
      mr->mesh_idx = -2;
      mr->is_emissive = false;
      logc("#### WARN: Gltf mesh %i (%s) has one or more invalid material "
           "indices.",
           j, gm->name);
    }
  }

  uint32_t dup_mesh_cnt = 0;

  // Count required emissive mesh duplicates by counting the nodes pointing to a
  // mesh
  logc("-------- Identifying emissive meshes that need to be duplicated");
  for(uint32_t i = 0; i < data.node_cnt; i++) {
    gltf_node *gn = &data.nodes[i];
    if(gn->mesh_idx >= 0) {
      mesh_ref *mr = &mesh_map[gn->mesh_idx];
      if(++mr->inst_cnt > 1 && mr->is_emissive) {
        logc("Node %i (%s) references a mesh that is emissive and requires "
             "duplication.",
             i, gn->name);
        dup_mesh_cnt++;
      }
    }
  }
  logc("Found that %i duplicates of emissive meshes are required.",
       dup_mesh_cnt);

  // Allocate scene
  logc("-------- Allocating %i meshes, %i materials, %i cameras and %i "
       "instances.",
       mesh_cnt + dup_mesh_cnt, data.mtl_cnt, max(1, data.cam_node_cnt),
       data.node_cnt - data.cam_node_cnt);
  scene_init(s, mesh_cnt + dup_mesh_cnt, data.mtl_cnt,
             max(1, data.cam_node_cnt), data.node_cnt - data.cam_node_cnt);

  // Gltf node indices to render mesh indices
  int32_t *render_mesh_ids = malloc(data.node_cnt * sizeof(*render_mesh_ids));

  // Reset mesh instance counters
  for(uint32_t i = 0; i < data.mesh_cnt; i++)
    mesh_map[i].inst_cnt = 0;

  logc("-------- Creating render meshes");
  for(uint32_t i = 0; i < data.node_cnt; i++) {
    gltf_node *gn = &data.nodes[i];
    if(gn->mesh_idx >= 0) {
      mesh_ref *mr = &mesh_map[gn->mesh_idx];
      if(mr->mesh_idx >= 0) {
        if(++mr->inst_cnt == 1 || mr->is_emissive) {
          gltf_mesh *gm = &data.meshes[gn->mesh_idx];
          mesh *m = scene_acquire_mesh(s);
          obj_type type = get_mesh_type(gm->name);
          if(type == OT_MESH || gm->prim_cnt > 1)
            load_mesh_data(m, &data, gm, bin);
          else
            generate_mesh_data(m, gm, mr->is_emissive);
          render_mesh_ids[i] = scene_attach_mesh(s, m, mr->is_emissive);
          if(mr->inst_cnt == 1)
            // Update to actual render mesh id, so all other (non-emissive)
            // instances get the index as well
            mr->mesh_idx = render_mesh_ids[i];
          logc(
              "Created render mesh %i from gltf mesh %i (%s) for node %i (%s).",
              render_mesh_ids[i], gn->mesh_idx, gm->name, i, gn->name);
        } else {
          render_mesh_ids[i] = mr->mesh_idx;
          logc("Found non-emissive instance of gltf mesh %i for node %i (%s) "
               "pointing at render mesh %i",
               gn->mesh_idx, i, gn->name, mr->mesh_idx);
        }
      } else {
        render_mesh_ids[i] = -2; // Not a valid mesh, so no render mesh
        logc("#### WARN: Skipping gltf mesh %i pointed to by node %i (%s) "
             "because the gltf has invalid data.",
             gn->mesh_idx, i, gn->name);
      }
    } else {
      render_mesh_ids[i] = -2; // Not a mesh node, so no render mesh
    }
  }

  free(mesh_map);

  // Add materials to scene
  logc("-------- Creating materials:");
  for(uint32_t i = 0; i < data.mtl_cnt; i++) {
    gltf_mtl *gm = &data.mtls[i];
    mtl m;
    create_mtl(&m, gm);
    scene_add_mtl(s, &m);
    logc("Created material %s (emissive: %i, refractive: %i)", gm->name,
         m.emissive > 0.0f, m.refractive == 1.0f);
  }

  // Initialize a default camera if there is no other camera in the scene
  if(data.cam_node_cnt == 0) {
    cam *c = scene_get_active_cam(s);
    *c = (cam){.vert_fov = 45.0f, .foc_dist = 10.0f, .foc_angle = 0.0f};
    cam_set(c, (vec3){0.0f, 0.0f, 10.0f}, (vec3){0.0f, 0.0f, -1.0f});
  }

  // Add nodes as scene instances
  logc("-------- Processing nodes and creating instances:");
  for(uint32_t i = 0; i < data.node_cnt; i++) {
    gltf_node *gn = &data.nodes[i];
    if(gn->mesh_idx >= 0)
      process_mesh_node(s, &data, gn, i, render_mesh_ids);
    else if(gn->cam_idx >= 0)
      process_cam_node(s, &data, gn);
    else
      logc("#### WARN: Found unknown node %i (%s). Expected camera or mesh. "
           "Skipping.",
           i, gn->name);
  }

  // Set scene attributes
  s->bg_col = data.bg_col;

  // Finalize the scene data (blas and ltris)
  logc("-------- Finalizing scene (blas and ltri creation)");
  scene_finalize(s);

  gltf_release(&data);

  logc("-------- Completed scene: %i meshes, %i materials, %i cameras, %i "
       "instances",
       s->mesh_cnt, s->mtl_cnt, s->cam_cnt, s->inst_cnt);

  return 0;
}
