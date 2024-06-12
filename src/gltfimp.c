#include "gltfimp.h"
#include "sutil.h"
#include "mutil.h"
#include "log.h"
#include "mtl.h"
#include "scene.h"

#define JSMN_PARENT_LINKS
#include "jsmn.h"

#define SBUF_LEN 1024
char sbuf[SBUF_LEN];

typedef enum prim_type {
  PT_GRID,
  PT_CUBE,
  PT_SPHERE,
  PT_CYLINDER,
  PT_MESH
} prim_type;

typedef enum bufview_type {
  BVT_SCALAR,
  BVT_VEC3
} bufview_type;

typedef struct gltf_prim {
  uint32_t pos_idx;
  uint32_t nrm_idx;
  uint32_t indices;
  uint32_t mtl_idx;
} gltf_prim;

typedef struct gltf_mesh {
  prim_type     type;
  uint32_t      subx;
  uint32_t      suby;
  gltf_prim*    prims;
  uint32_t      prim_cnt;
} gltf_mesh;

typedef struct gltf_accessor {
  uint32_t      bufview;
  uint32_t      comp_type;
  uint32_t      count;
  bufview_type  type;
} gltf_accessor;

typedef struct gltf_bufview {
  uint32_t      buf; // Only 1 supported, which is the .bin file that comes with the .gltf
  uint32_t      byte_len;
  uint32_t      byte_ofs;
  uint32_t      target;
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
    logc("o >>");
    int j = 1;
    for(int i=0; i<t->size; i++) {
      jsmntok_t *key = t + j;
      j += dump(s, key);
      if(key->size > 0)
        j += dump(s, t + j);
    }
    logc("<< o");
    return j;
  }

  if (t->type == JSMN_ARRAY) {
    logc("a >>");
    int j = 1;
    for(int i=0; i<t->size; i++)
      j += dump(s, t + j);
    logc("<< a");
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
      p->indices = atoi(toktostr(s, &t[j + 1]));
      logc("indices: %i", p->indices);
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

int read_mesh(gltf_mesh *m, const char *s, jsmntok_t *t)
{
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "extras") == 0) {
      j += 1 + read_extras(m, s, t + j + 1);
      continue;
    }

    if(jsoneq(s, key, "name") == 0) {
      char *name = toktostr(s, &t[j + 1]);
      if(strstr(name, "Grid"))
        m->type = PT_GRID;
      else if(strstr(name, "Cube"))
        m->type = PT_CUBE;
      else if(strstr(name, "Sphere"))
        m->type = PT_SPHERE;
      else if(strstr(name, "Cylinder"))
        m->type = PT_CYLINDER;
      else
        m->type = PT_MESH;
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
  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *key = t + j;

    if(jsoneq(s, key, "bufferView") == 0) {
      a->bufview = atoi(toktostr(s, &t[j + 1]));
      logc("bufferView: %i", a->bufview);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "componentType") == 0) {
      a->comp_type = atoi(toktostr(s, &t[j + 1]));
      logc("componentType: %i", a->comp_type);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "count") == 0) {
      a->count = atoi(toktostr(s, &t[j + 1]));
      logc("count: %i", a->count);
      j += 2;
      continue;
    }

    if(jsoneq(s, key, "type") == 0) {
      char *type = toktostr(s, &t[j + 1]);
      bool err = false;
      if(strstr(type, "VEC3"))
        a->type = BVT_VEC3;
      else if(strstr(type, "SCALAR"))
        a->type = BVT_SCALAR;
      else {
        logc("Unknown buffer view type: %s", type);
        err = true;
      }
      if(!err) {
        logc("type: %i (%s)", a->type, type);
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

    if(jsoneq(s, key, "target") == 0) {
      b->target = atoi(toktostr(s, &t[j + 1]));
      logc("target: %i", b->target);
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

int gltf_import(scene *s, const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz)
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

    // Ignore
    logc("////////");
    j += dump(gltf, k);
    if(k->size > 0) {
      j += dump(gltf, t + j);
    }
    logc("\\\\\\\\\\\\\\\\");
  }

  // Combine meshes/accessors/bufviews to actual scene meshes
  // Ignore primitives we can generate or built-intersect
  // Do not ignore if mtl is emissive (these must be generated or loaded meshes)

  // Read nodes (instances)

  free(data.meshes);
  free(data.accessors);
  free(data.bufviews);

  return 0;
}
