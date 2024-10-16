#include "gltf.h"

#include "../base/log.h"
#include "../base/math.h"
#include "../base/string.h"
#include "../base/walloc.h"

#define JSMN_PARENT_LINKS
#include "../ext/jsmn.h"

#define SBUF_LEN 1024 // Fixed buffer for temporary string storage
char sbuf[SBUF_LEN];

// Make silent
#undef logc
#define logc(...) ((void)0)

char *bstrndup(char *dest, const char *src, size_t len, size_t max_len)
{
  uint32_t l = min(len, max_len - 1);
  for(uint32_t i = 0; i < l; i++)
    dest[i] = *src++;
  dest[l] = '\0';
  return dest;
}

char *toktostr(const char *s, jsmntok_t *t)
{
  if(t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE)
    return bstrndup(sbuf, s + t->start, t->end - t->start, SBUF_LEN);
  logc("Expected token with type string");
  return NULL;
}

int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if(tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
     strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

uint32_t dump(const char *s, jsmntok_t *t)
{
  if(t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING) {
    logc("// %.*s", t->end - t->start, s + t->start);
    return 1;
  }

  if(t->type == JSMN_OBJECT) {
    // logc("o >>");
    uint32_t j = 1;
    for(int i = 0; i < t->size; i++) {
      jsmntok_t *key = t + j;
      j += dump(s, key);
      if(key->size > 0)
        j += dump(s, t + j);
    }
    // logc("<< o");
    return j;
  }

  if(t->type == JSMN_ARRAY) {
    // logc("a >>");
    uint32_t j = 1;
    for(int i = 0; i < t->size; i++)
      j += dump(s, t + j);
    // logc("<< a");
    return j;
  }

  return 0;
}

uint32_t ignore(const char *s, jsmntok_t *t)
{
  uint32_t j = dump(s, t);
  if(t->size > 0)
    j += dump(s, t + j);
  return j;
}

uint32_t read_mtl_extensions(gltf_mtl *m, const char *s, jsmntok_t *t,
                             float *emissive_strength)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
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
        logc("transmissionFactor (refractive): %f", m->refractive);
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_pbr_metallic_roughness(gltf_mtl *m, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "baseColorFactor") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 4) {
        // Just read rgb, ignore alpha component (= j + 5)
        m->col =
            (vec3){atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])),
                   atof(toktostr(s, &t[j + 4]))};
        // vec3_logc("baseColorFactor: ", m->col);
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_mtl(gltf_mtl *m, const char *s, jsmntok_t *t)
{
  m->col = (vec3){1.0f, 1.0f, 1.0f};
  m->metallic = 0.0f;
  m->roughness = 0.5f;
  m->ior = 1.5f;
  m->refractive = 0.0f;

  // Temporary store only, will use to calc material emission
  float emissive_strength = 0.0f;
  vec3 emissive_factor = (vec3){1.0f, 1.0f, 1.0f};

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      bstrndup(m->name, name, t[j + 1].end - t[j + 1].start, NAME_STR_LEN);
      logc("name: %s", m->name);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "emissiveFactor") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 3) {
        emissive_factor =
            (vec3){atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])),
                   atof(toktostr(s, &t[j + 4]))};
        // vec3_logc("emissiveFactor: ", emissive_factor);
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

    j += ignore(s, key);
  }

  m->emission = vec3_scale(emissive_factor, emissive_strength);

  return j;
}

uint32_t read_mtls(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> mtls");

  data->mtl_cnt = t->size;
  data->mtls = malloc(data->mtl_cnt * sizeof(*data->mtls));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> mtl %i", i);
    j += read_mtl(&data->mtls[cnt++], s, t + j);
    logc("<<<< mtl %i", i);
  }

  logc("<< mtls (total: %i)", cnt);

  return j;
}

uint32_t read_node_extras(gltf_node *n, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "invisible") == 0) {
      n->invisible = true;
      logc("invisible: %i", n->invisible);
      j += 2;
      continue;
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_node(gltf_node *n, const char *s, jsmntok_t *t)
{
  n->mesh_idx = -1;
  n->cam_idx = -1;
  n->invisible = false;
  n->trans = (vec3){0.0f, 0.0f, 0.0f};
  n->scale = (vec3){1.0f, 1.0f, 1.0f};
  memcpy(n->rot, (float[]){0.0f, 0.0f, 0.0f, 1.0f}, sizeof(n->rot));

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "extras") == 0) {
      j += 1 + read_node_extras(n, s, t + j + 1);
      continue;
    }

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      bstrndup(n->name, name, t[j + 1].end - t[j + 1].start, NAME_STR_LEN);
      logc("name: %s", name);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "mesh") == 0) {
      n->mesh_idx = atoi(toktostr(s, &t[j + 1]));
      logc("mesh: %i", n->mesh_idx);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "camera") == 0) {
      n->cam_idx = atoi(toktostr(s, &t[j + 1]));
      logc("camera: %i", n->cam_idx);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "translation") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 3) {
        n->trans =
            (vec3){atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])),
                   atof(toktostr(s, &t[j + 4]))};
        // vec3_logc("translation: ", n->trans);
        j += 5;
        continue;
      } else
        logc("Failed to read translation. Expected vec3.");
    }

    if(jsoneq(s, key, "scale") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 3) {
        n->scale =
            (vec3){atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])),
                   atof(toktostr(s, &t[j + 4]))};
        // vec3_logc("scale: ", n->scale);
        j += 5;
        continue;
      } else
        logc("Failed to read scale. Expected vec3.");
    }

    if(jsoneq(s, key, "rotation") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 4) {
        n->rot[0] = atof(toktostr(s, &t[j + 2]));
        n->rot[1] = atof(toktostr(s, &t[j + 3]));
        n->rot[2] = atof(toktostr(s, &t[j + 4]));
        n->rot[3] = atof(toktostr(s, &t[j + 5]));
        logc("rotation: %f, %f, %f, %f", n->rot[0], n->rot[1], n->rot[2],
             n->rot[3]);
        j += 6;
        continue;
      } else
        logc("Failed to read rotation. Expected quaternion (xyzw).");
    }

    if(jsoneq(s, key, "children") == 0) {
      logc("#### ERROR: Gltf nodes with children not supported. Most likely "
           "this gltf file will not be read correctly.");
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_nodes(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> nodes");

  data->node_cnt = t->size;
  data->nodes = malloc(data->node_cnt * sizeof(*data->nodes));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> node %i", i);

    gltf_node *n = &data->nodes[cnt++];
    j += read_node(n, s, t + j);

    if(n->cam_idx >= 0)
      data->cam_node_cnt++;

    logc("<<<< node %i", i);
  }

  logc("<< nodes (total: %i)", cnt);

  return j;
}

uint32_t read_cam_perspective(gltf_cam *c, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "yfov") == 0) {
      c->vert_fov = atof(toktostr(s, &t[j + 1]));
      logc("yfov: %f", c->vert_fov);
      j += 2;
      continue;
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_cam(gltf_cam *c, const char *s, jsmntok_t *t)
{
  c->vert_fov = 45.0f;

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      bstrndup(c->name, name, t[j + 1].end - t[j + 1].start, NAME_STR_LEN);
      logc("name: %s", c->name);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "perspective") == 0) {
      j += 1 + read_cam_perspective(c, s, t + j + 1);
      continue;
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_cams(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> cams");

  data->cam_cnt = t->size;
  data->cams = malloc(data->cam_cnt * sizeof(*data->cams));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> cam %i", i);
    j += read_cam(&data->cams[cnt++], s, t + j);
    logc("<<<< cam %i", i);
  }

  logc("<< cams (total: %i)", cnt);

  return j;
}

uint32_t read_mesh_extras(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
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

    if(jsoneq(s, key, "innerradius") == 0) {
      m->in_radius = atof(toktostr(s, &t[j + 1]));
      logc("inner radius: %i", m->in_radius);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "nocaps") == 0) {
      m->no_caps = true;
      logc("no caps: %i", m->no_caps);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "facenormals") == 0) {
      m->face_nrms = true;
      logc("facenormals: %i", m->face_nrms);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "invertnormals") == 0) {
      m->invert_nrms = true;
      logc("invertnormals: %i", m->invert_nrms);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "share") == 0) {
      // Share id 0 is reserved for not set
      m->share_id = 1 + atoi(toktostr(s, &t[j + 1]));
      logc("share id: %i", m->share_id);
      j += 2;
      continue;
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_attributes(gltf_prim *p, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_primitive(gltf_prim *p, const char *s, jsmntok_t *t)
{
  p->ind_idx = -1;
  p->mtl_idx = -1;
  p->pos_idx = -1;
  p->nrm_idx = -1;

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_primitives(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  logc(">>>> primitives");

  m->prim_cnt = t->size;
  m->prims = malloc(m->prim_cnt * sizeof(*m->prims));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>>>> primitive %i", i);
    j += read_primitive(&m->prims[cnt++], s, &t[j]);
    logc("<<<<<< primitive %i", i);
  }

  logc("<<<< primitives (total: %i)", cnt);

  return j;
}

uint32_t read_mesh(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  m->prims = NULL;
  m->prim_cnt = 0;
  m->subx = 0;
  m->suby = 0;
  m->in_radius = 0.0f;
  m->no_caps = false;
  m->face_nrms = false;
  m->invert_nrms = false;
  m->share_id = 0;

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "extras") == 0) {
      j += 1 + read_mesh_extras(m, s, t + j + 1);
      continue;
    }

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      bstrndup(m->name, name, t[j + 1].end - t[j + 1].start, NAME_STR_LEN);
      logc("name: %s", name);
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_meshes(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> meshes");

  data->mesh_cnt = t->size;
  data->meshes = malloc(data->mesh_cnt * sizeof(*data->meshes));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> mesh %i", i);
    j += read_mesh(&data->meshes[cnt++], s, t + j);
    logc("<<<< mesh %i", i);
  }

  logc("<< meshes (total: %i)", cnt);

  return j;
}

uint32_t read_accessor(gltf_accessor *a, const char *s, jsmntok_t *t)
{
  a->bufview = -1;
  a->byte_ofs = 0;

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "bufferView") == 0) {
      a->bufview = atoi(toktostr(s, &t[j + 1]));
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
      if(strstr(type, "VEC3"))
        a->data_type = DT_VEC3;
      else if(strstr(type, "SCALAR"))
        a->data_type = DT_SCALAR;
      else {
        a->data_type = DT_UNKNOWN;
        logc("Accessor with unknown data type: %s", type);
      }
      logc("type: %i (%s)", a->data_type, type);
      j += 2;
      continue;
    }

    j += ignore(s, key);
  }

  if(a->bufview < 0)
    logc("#### ERROR: Undefined buffer view found. This is not supported.");

  return j;
}

uint32_t read_accessors(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> accessors");

  data->accessor_cnt = t->size;
  data->accessors = malloc(data->accessor_cnt * sizeof(*data->accessors));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> accessor %i", i);
    j += read_accessor(&data->accessors[cnt++], s, t + j);
    logc("<<<< accessor %i", i);
  }

  logc("<< accessors (total: %i)", cnt);

  return j;
}

uint32_t read_bufview(gltf_bufview *b, const char *s, jsmntok_t *t)
{
  b->byte_stride = 0;
  b->byte_ofs = 0;

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
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

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_bufviews(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> bufviews");

  data->bufview_cnt = t->size;
  data->bufviews = malloc(data->bufview_cnt * sizeof(*data->bufviews));

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> bufview %i", i);
    j += read_bufview(&data->bufviews[cnt++], s, t + j);
    logc("<<<< bufview %i", i);
  }

  logc("<< bufviews (total: %i)", cnt);

  return j;
}

uint32_t read_scene_extras(gltf_data *d, const char *s, jsmntok_t *t)
{
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "bgcol") == 0) {
      if(t[j + 1].type == JSMN_ARRAY && t[j + 1].size == 3) {
        d->bg_col =
            (vec3){atof(toktostr(s, &t[j + 2])), atof(toktostr(s, &t[j + 3])),
                   atof(toktostr(s, &t[j + 4]))};
        // vec3_logc("bg col: ", d->bg_col);
        j += 5;
        continue;
      } else
        logc("Failed to read bg col. Expected vec3.");
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_scene(gltf_data *d, const char *s, jsmntok_t *t)
{
  d->bg_col = (vec3){0.0f, 0.0f, 0.0f};

  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "extras") == 0) {
      j += 1 + read_scene_extras(d, s, t + j + 1);
      continue;
    }

    j += ignore(s, key);
  }

  return j;
}

uint32_t read_scenes(gltf_data *data, const char *s, jsmntok_t *t)
{
  logc(">> scenes");

  if(t->size > 1)
    logc("#### WARN: Found %i scenes. Will process only the first and skip all "
         "others.",
         t->size);

  uint32_t cnt = 0;
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    logc(">>>> scene %i", i);

    // For now we process the first scene only
    if(i == 0)
      j += read_scene(data, s, t + j);
    else
      j += ignore(s, t + j);

    logc("<<<< scene %i", i);
  }

  logc("<< scenes (total: %i, read: %i)", t->size, cnt);

  return j;
}

uint8_t gltf_read(gltf_data *data, const char *gltf, size_t gltf_sz)
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
    free(t);
    return 1;
  }

  // First token should always be an object
  if(cnt < 1 || t[0].type != JSMN_OBJECT) {
    logc("Expected object as root token in gltf");
    free(t);
    return 1;
  }

  // Read token/data
  uint32_t j = 1;
  for(int i = 0; i < t->size; i++) {
    jsmntok_t *key = t + j;

    // Scenes
    if(jsoneq(gltf, key, "scenes") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_scenes(data, gltf, t + j);
      continue;
    }

    // Materials
    if(jsoneq(gltf, key, "materials") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_mtls(data, gltf, t + j);
      continue;
    }

    // Nodes
    if(jsoneq(gltf, key, "nodes") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_nodes(data, gltf, t + j);
      continue;
    }

    // Camera
    if(jsoneq(gltf, key, "cameras") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_cams(data, gltf, t + j);
      continue;
    }

    // Meshes
    if(jsoneq(gltf, key, "meshes") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_meshes(data, gltf, t + j);
      continue;
    }

    // Accessors
    if(jsoneq(gltf, key, "accessors") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_accessors(data, gltf, t + j);
      continue;
    }

    // Buffer views
    if(jsoneq(gltf, key, "bufferViews") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size > 0) {
      j++;
      j += read_bufviews(data, gltf, t + j);
      continue;
    }

    // Buffers. Check that we have a single buffer with mesh data. Something
    // else is not supported ATM.
    if(jsoneq(gltf, key, "buffers") == 0 && t[j + 1].type == JSMN_ARRAY &&
       t[j + 1].size != 1) {
      logc("#### ERROR: Expected gltf with one buffer only. Can not process "
           "file further.");
      free(t);
      return 1;
    }

    // Not reading/interpreting:
    // - Assets, extensions
    // - Default scene (expecting only one scene)
    // - Root nodes or node hierarchy (expecting flat list of nodes)
    // - All typs of animation/skinning etc.
    // - Texture coords and textures of all types
    // - Buffers (expecting only one buffer)
    j += ignore(gltf, key);
  }

  return 0;
}

void gltf_release(gltf_data *data)
{
  for(uint32_t i = 0; i < data->mesh_cnt; i++)
    free(data->meshes[i].prims);

  free(data->meshes);
  free(data->mtls);
  free(data->nodes);
  free(data->accessors);
  free(data->bufviews);
}
