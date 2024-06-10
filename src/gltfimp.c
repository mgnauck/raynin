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

  int j = 1;
  for(int i=0; i<t->size; i++) {
    jsmntok_t *k = t + j;

    // Materials
    if(jsoneq(gltf, k, "materials") == 0 && t[j + 1].type == JSMN_ARRAY && t[j + 1].size > 0) {
      j++;
      j += read_mtls(s, gltf, t + j);
      continue;
    }

    // Meshes

    // Nodes

    // Ignore
    logc("////////");
    j += dump(gltf, k);
    if(k->size > 0) {
      j += dump(gltf, t + j);
    }
    logc("\\\\\\\\\\\\\\\\");
  }

  return 0;
}
