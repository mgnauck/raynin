#include "gltfimp.h"
#include "sutil.h"
#include "mutil.h"
#include "log.h"
#include "scene.h"
#include "mesh.h"
#include "bvh.h"
#include "mtl.h"
#include "tri.h"

#define JSMN_PARENT_LINKS
#include "jsmn.h"

#define SBUF_LEN 1024
char sbuf[SBUF_LEN];

typedef enum obj_type {
  OT_GRID,
  OT_CUBE,
  OT_SPHERE,
  OT_CYLINDER,
  OT_MESH
} obj_type;

typedef enum data_type {
  DT_SCALAR,
  DT_VEC3
} data_type;

typedef struct gltf_prim {
  uint32_t pos_idx;
  uint32_t nrm_idx;
  uint32_t ind_idx;
  uint32_t mtl_idx;
} gltf_prim;

typedef struct gltf_mesh {
  obj_type      type;
  uint32_t      subx;
  uint32_t      suby;
  gltf_prim*    prims;
  uint32_t      prim_cnt;
  int32_t       mesh_idx;   // Will be the index of the final mesh
} gltf_mesh;

typedef struct gltf_accessor {
  uint32_t      bufview;    // TODO: When undefined, the data MUST be initialized with zeros.
  uint32_t      count;
  uint32_t      byte_ofs;   // Optional
  uint32_t      comp_type;
  data_type     data_type;
} gltf_accessor;

typedef struct gltf_bufview {
  uint32_t      buf;        // Only 1 supported, which is the .bin file that comes with the .gltf
  uint32_t      byte_len;
  uint32_t      byte_ofs;   // Optional
  uint32_t      byte_stride;
} gltf_bufview;

typedef struct gltf_data {
  gltf_mesh     *meshes;
  uint32_t      mesh_cnt;
  gltf_accessor *accessors;
  uint32_t      accessor_cnt;
  gltf_bufview  *bufviews;
  uint32_t      bufview_cnt;
} gltf_data;

char *bstrndup(const char *str, size_t len)
{
  for(int i=0; i<(int)min(len, SBUF_LEN - 1); i++)
    sbuf[i] = *str++;
  sbuf[min(len, SBUF_LEN - 1)] = '\0';
  return sbuf;
}

char *toktostr(const char *s, jsmntok_t *t)
{
  if(t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE)
    return bstrndup(s + t->start, t->end - t->start);
  logc("Expected token with type string");
  return NULL;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if(tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

int dump(const char *s, jsmntok_t *t)
{
  if(t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING) {
    logc("// %.*s", t->end - t->start, s + t->start);
    return 1;
  }

  if(t->type == JSMN_OBJECT) {
    //logc("o >>");
    int j = 1;
    for(int i=0; i<t->size; i++) {
      jsmntok_t *key = t + j;
      j += dump(s, key);
      if(key->size > 0)
        j += dump(s, t + j);
    }
    //logc("<< o");
    return j;
  }

  if (t->type == JSMN_ARRAY) {
    //logc("a >>");
    int j = 1;
    for(int i=0; i<t->size; i++)
      j += dump(s, t + j);
    //logc("<< a");
    return j;
  }

  return 0;
}

int read_mtl_extensions(mtl *m, const char *s, jsmntok_t *t, float *emissive_strength)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "KHR_materials_emissive_strength") == 0) {
      if(jsoneq(s, t + j + 2, "emissiveStrength") == 0) {
        *emissive_strength = atof(toktostr(s, t + j + 3));
        logc("emissiveStrength: %f", *emissive_strength);
        j += 4;
        continue;
      }
    }

    if(jsoneq(s, key, "KHR_materials_transmission") == 0) {
      if(jsoneq(s, t + j + 2, "transmissionFactor") == 0) {
        m->refractive = atof(toktostr(s, t + j + 3));
        logc("transmissionFactor: %f", m->refractive);
        j += 4;
        continue;
      }
    }

    if(jsoneq(s, key, "KHR_materials_ior") == 0) {
      if(jsoneq(s, t + j + 2, "ior") == 0) {
        m->ior = atof(toktostr(s, t + j + 3));
        logc("ior: %f", m->ior);
        j += 4;
        continue;
      }
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_pbr_metallic_roughness(mtl *m, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "baseColorFactor") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 4) {
        // Just read rgb, ignore alpha component (= j + 5)
        m->col = (vec3){ atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])), atof(toktostr(s, &t[j + 4])) };
        vec3_logc("baseColorFactor: ", m->col);
        j += 6;
        continue;
      } else
        logc("Failed to read baseColorFactor");
    }

    if(jsoneq(s, key, "metallicFactor") == 0) {
      if(t[j + 1].type == JSMN_PRIMITIVE) {
        m->metallic = atof(toktostr(s, &t[j + 1]));
        logc("metallicFactor: %f", m->metallic);
        j += 2;
        continue;
      } else
        logc("Failed to read metallicFactor");
    }

    if(jsoneq(s, key, "roughnessFactor") == 0) {
      if(t[j + 1].type == JSMN_PRIMITIVE) {
        m->roughness = atof(toktostr(s, &t[j + 1]));
        logc("roughnessFactor: %f", m->roughness);
        j += 2;
        continue;
      } else
        logc("Failed to read roughnessFactor");
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_mtl(mtl *m, const char *s, jsmntok_t *t)
{
  float emissive_strength = 1.0f;
  vec3 emissive_factor = (vec3){ 0.0f, 0.0f, 0.0f };

  mtl_set_defaults(m);

  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "emissiveFactor") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 3) {
        emissive_factor = (vec3){ atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])), atof(toktostr(s, &t[j + 4])) };
        vec3_logc("emissiveFactor: ", emissive_factor);
        j += 5;
        continue;
      } else
        logc("Failed to read emissiveFactor");
    }

    if(jsoneq(s, key, "extensions") == 0) {
      j += 1 + read_mtl_extensions(m, s, t + j + 1, &emissive_strength);
      continue;
    }

    if(jsoneq(s, key, "pbrMetallicRoughness") == 0) {
      j += 1 + read_pbr_metallic_roughness(m, s, t + j + 1);
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  if(vec3_max_comp(vec3_scale(emissive_factor, emissive_strength)) > 1.0f) {
    m->col = vec3_scale(emissive_factor, emissive_strength);
    vec3_logc("Adjusted mtl base col to emissive: ", m->col);
  }

  return j;
}

int read_mtls(scene *scene, const char *s, jsmntok_t *t)
{
  logc(">> mtls");

  scene->mtls = malloc(t->size * sizeof(*scene->mtls));
  scene->mtl_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>> mtl %i", i);
    mtl new_mtl;
    j += read_mtl(&new_mtl, s, t + j);
    scene_add_mtl(scene, &new_mtl);
    logc("<<<< mtl %i", i);
  }

  logc("<< mtls (total: %i)", scene->mtl_cnt);

  return j;
}

int read_cam_perspective(cam *c, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "yfov") == 0) {
      c->vert_fov = atof(toktostr(s, &t[j + 1]));
      logc("yfov: %f", c->vert_fov);
      j += 2;
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_cam(cam *c, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "perspective") == 0) {
      j += 1 + read_cam_perspective(c, s, t + j + 1);
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_cams(scene *scene, const char *s, jsmntok_t *t)
{
  logc(">> cams");

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>> cam %i", i);
    if(i == 0) {
      // Read parameters of first camera only
      j += read_cam(&scene->cam, s, t + j);
    } else {
      j += dump(s, t + j);
    }
    logc("<<<< cam %i", i);
  }

  logc("<< cams (total: %i)", t->size);

  return j;
}

int read_extras(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "subx") == 0) {
      m->subx = atoi(toktostr(s, &t[j + 1]));
      logc("subx: %i", m->subx);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "suby") == 0) {
      m->suby = atoi(toktostr(s, &t[j + 1]));
      logc("suby: %i", m->suby);
      j += 2;
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_attributes(gltf_prim *p, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "POSITION") == 0) {
      p->pos_idx = atoi(toktostr(s, &t[j + 1]));
      logc("position: %i", p->pos_idx);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "NORMAL") == 0) {
      p->nrm_idx = atoi(toktostr(s, &t[j + 1]));
      logc("normal: %i", p->nrm_idx);
      j += 2;
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_primitive(gltf_prim *p, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "attributes") == 0) {
      j += 1 + read_attributes(p, s, t + j + 1);
      continue;
    }

    if(jsoneq(s, key, "indices") == 0) {
      p->ind_idx = atoi(toktostr(s, &t[j + 1]));
      logc("indices: %i", p->ind_idx);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "material") == 0) {
      p->mtl_idx = atoi(toktostr(s, &t[j + 1]));
      logc("material: %i", p->mtl_idx);
      j += 2;
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}


int read_primitives(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  logc(">>>> primitives");

  m->prims = malloc(t->size * sizeof(*m->prims));
  m->prim_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>>>> primitive %i", i);
    j += read_primitive(&m->prims[m->prim_cnt++], s, &t[j]);
    logc("<<<<<< primitive %i", i);
  }

  logc("<<<< primitives (total: %i)", m->prim_cnt);

  return j;
}

obj_type get_type(const char *name)
{
  if(strstr(name, "Grid"))
    return OT_GRID;
  else if(strstr(name, "Cube"))
    return OT_CUBE;
  else if(strstr(name, "Sphere"))
    return OT_SPHERE;
  else if(strstr(name, "Cylinder"))
    return OT_CYLINDER;
  return OT_MESH;
}

int read_mesh(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  m->mesh_idx = -1; // No real mesh assigned yet

  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "extras") == 0) {
      j += 1 + read_extras(m, s, t + j + 1);
      continue;
    }

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      m->type = get_type(name);
      logc("type: %i (%s)", m->type, name);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "primitives") == 0) {
      if(t[j + 1].type == JSMN_ARRAY) {
        j += 1 + read_primitives(m, s, &t[j + 1]);
        continue;
      } else
        logc("Failed to read primitives");
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_meshes(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> meshes");

  data->meshes = malloc(t->size * sizeof(*data->meshes));
  data->mesh_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>> mesh %i", i);
    j += read_mesh(&data->meshes[data->mesh_cnt++], s, t + j);
    logc("<<<< mesh %i", i);
  }

  logc("<< meshes (total: %i)", data->mesh_cnt);

  return j;
}

int read_accessor(gltf_accessor *a, const char *s, jsmntok_t *t)
{
  a->byte_ofs = 0;

  int j = 1;
  bool bv_contained = false;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "bufferView") == 0) {
      a->bufview = atoi(toktostr(s, &t[j + 1]));
      bv_contained = true;
      logc("bufferView: %i", a->bufview);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "count") == 0) {
      a->count = atoi(toktostr(s, &t[j + 1]));
      logc("count: %i", a->count);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "byteOffset") == 0) {
      a->byte_ofs = atoi(toktostr(s, &t[j + 1]));
      logc("byteOffset: %i", a->byte_ofs);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "componentType") == 0) {
      a->comp_type = atoi(toktostr(s, &t[j + 1]));
      logc("componentType: %i", a->comp_type);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "type") == 0) {
      char *type = toktostr(s, &t[j + 1]);
      bool err = false;
      if(strstr(type, "VEC3"))
        a->data_type = DT_VEC3;
      else if(strstr(type, "SCALAR"))
        a->data_type = DT_SCALAR;
      else {
        logc("Accessor with unknown data type: %s", type);
        err = true;
      }
      if(!err) {
        logc("type: %i (%s)", a->data_type, type);
        j += 2;
      }
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  if(!bv_contained)
    logc("#### ERROR: Undefined buffer view found. This is not supported.");

  return j;
}

int read_accessors(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> accessors");

  data->accessors = malloc(t->size * sizeof(*data->accessors));
  data->accessor_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>> accessor %i", i);
    j += read_accessor(&data->accessors[data->accessor_cnt++], s, t + j);
    logc("<<<< accessor %i", i);
  }

  logc("<< accessors (total: %i)", data->accessor_cnt);

  return j;
}

int read_bufview(gltf_bufview *b, const char *s, jsmntok_t *t)
{
  b->byte_stride = 0;
  b->byte_ofs = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "buffer") == 0) {
      b->buf = atoi(toktostr(s, &t[j + 1]));
      logc("buffer: %i", b->buf);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "byteLength") == 0) {
      b->byte_len = atoi(toktostr(s, &t[j + 1]));
      logc("byteLength: %i", b->byte_len);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "byteOffset") == 0) {
      b->byte_ofs = atoi(toktostr(s, &t[j + 1]));
      logc("count: %i", b->byte_ofs);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "byteStride") == 0) {
      b->byte_stride = atoi(toktostr(s, &t[j + 1]));
      logc("byteStride: %i", b->byte_stride);
      j += 2;
      continue;
    }

    // Ignore
    j += dump(s, key);
    if(key->size > 0) {
      j += dump(s, t + j);
    }
  }

  return j;
}

int read_bufviews(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> bufviews");

  data->bufviews = malloc(t->size * sizeof(*data->bufviews));
  data->bufview_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    logc(">>>> bufview %i", i);
    j += read_bufview(&data->bufviews[data->bufview_cnt++], s, t + j);
    logc("<<<< bufview %i", i);
  }

  logc("<< bufviews (total: %i)", data->bufview_cnt);

  return j;
}

uint8_t read_byte(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(uint8_t *)(buf + byte_ofs + sizeof(uint8_t) * i);
}

uint16_t read_short(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
{
  return *(uint16_t *)(buf + byte_ofs + sizeof(uint16_t) * i);
}

uint32_t read_int(const uint8_t *buf, uint32_t byte_ofs, uint32_t i)
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

void create_mesh(scene *s, mesh *m, gltf_data *d, gltf_mesh *gm, const uint8_t *bin)
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
        read_byte(bin, ind_bv->byte_ofs, i + 0) : (ind_acc->comp_type == 5123) ?
          read_short(bin, ind_bv->byte_ofs, i + 0) : read_int(bin, ind_bv->byte_ofs, i + 0);

      uint32_t i1 = (ind_acc->comp_type == 5121) ?
        read_byte(bin, ind_bv->byte_ofs, i + 1) : (ind_acc->comp_type == 5123) ?
          read_short(bin, ind_bv->byte_ofs, i + 1) : read_int(bin, ind_bv->byte_ofs, i + 1);

      uint32_t i2 = (ind_acc->comp_type == 5121) ?
        read_byte(bin, ind_bv->byte_ofs, i + 2) : (ind_acc->comp_type == 5123) ?
          read_short(bin, ind_bv->byte_ofs, i + 2) : read_int(bin, ind_bv->byte_ofs, i + 2);

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

    m->is_emissive |= mtl_is_emissive(&s->mtls[p->mtl_idx]);
  }

  logc("Created mesh with %i triangles", tri_cnt);

  if(m->tri_cnt != tri_cnt)
    logc("#### WARN: Mesh is missing some triangle data, i.e. skipped gltf primitve.");

  // In case we skipped a primitive, adjust the tri cnt of the mesh
  m->tri_cnt = tri_cnt;
  mesh_finalize(s, m);
}

uint8_t gltf_import(scene *s, const char *gltf, size_t gltf_sz, const uint8_t *bin, size_t bin_sz)
{
  jsmn_parser parser;
  jsmn_init(&parser);

  // Retrieve number of tokens
  int cnt = jsmn_parse(&parser, gltf, gltf_sz, NULL, 0);
  if(cnt < 0) {
    logc("Something went wrong parsing the token count of the gltf: %i", cnt);
    return 1;
  }

  // Parse all tokens
  jsmntok_t *t = malloc(cnt * sizeof(*t));
  jsmn_init(&parser);
  cnt = jsmn_parse(&parser, gltf, gltf_sz, t, cnt);
  if(cnt < 0) {
    logc("Something went wrong parsing the gltf: %i", cnt);
    return 1;
  }

  // First token is always an object
  if(cnt < 1 || t[0].type != JSMN_OBJECT) {
    logc("Expected object as root token in gltf");
    return 1;
  }

  // Temporary gltf data
  gltf_data data;
  data.mesh_cnt = 0;
  data.accessor_cnt = 0;
  data.bufview_cnt = 0;

  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *k = t + j;

    // Materials
    if(jsoneq(gltf, k, "materials") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_mtls(s, gltf, t + j);
      continue;
    }

    // Cameras (read parameters of first camera only)
    if(jsoneq(gltf, k, "cameras") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_cams(s, gltf, t + j);
      continue;
    }

    // Meshes
    if(jsoneq(gltf, k, "meshes") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_meshes(&data, gltf, t + j);
      continue;
    }

    // Accessors
    if(jsoneq(gltf, k, "accessors") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_accessors(&data, gltf, t + j);
      continue;
    }

    // Buffer views
    if(jsoneq(gltf, k, "bufferViews") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_bufviews(&data, gltf, t + j);
      continue;
    }

    // Buffers. Check that we have a single buffer with mesh data. Something else is not supported ATM.
    if(jsoneq(gltf, k, "buffers") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size != 1) {
      logc("Expected gltf with only one buffer. Can not process file further.");
      return 1;
    }

    // Ignore
    j += dump(gltf, k);
    if(k->size > 0) {
      j += dump(gltf, t + j);
    }
  }

  // Identify actual meshes
  for(uint32_t j=0; j<data.mesh_cnt; j++) {
    gltf_mesh *gm = &data.meshes[j];
    for(uint32_t i=0; i<gm->prim_cnt; i++) {
      gltf_prim *gp = &gm->prims[i]; 
      /*if(mtl_is_emissive(&s->mtls[gp->mtl_idx]) || gm->type == OT_MESH)*/ {
        gm->mesh_idx = s->mesh_cnt++;
        break;
      }
    }
  }

  logc("Found %i actual meshes in the scene", s->mesh_cnt);

  s->meshes = malloc(s->mesh_cnt * sizeof(*s->meshes));
  s->bvhs = malloc(s->mesh_cnt * sizeof(*s->bvhs));

  // TODO Stride handling
  // TODO Generated meshes, i.e. (gm->mesh_idx > 0 && gm->type != OT_MESH)

  // Populate meshes with gltf mesh data
  for(uint32_t i=0; i<data.mesh_cnt; i++) {
    gltf_mesh *gm = &data.meshes[i];
    if(gm->mesh_idx >= 0)
      create_mesh(s, &s->meshes[gm->mesh_idx], &data, gm, bin);
  }

  // Read nodes (instances)

  free(data.meshes);
  free(data.accessors);
  free(data.bufviews);

  return 0;
}
