#include "export.h"

#include "../base/log.h"
#include "../base/walloc.h"
#include "../scene/mtl.h"
#include "ecam.h"
#include "einst.h"
#include "emesh.h"
#include "escene.h"
#include "ieutil.h"

// Make silent
#undef logc
#define logc(...) ((void)0)

typedef struct share_data {
  uint8_t share_id;
  uint32_t vertices_ofs;
  uint32_t normals_ofs;
  uint32_t indices_ofs;
} share_data;

share_data *find_shared(uint8_t share_id, share_data *shared,
                        uint32_t *share_cnt)
{
  if(share_id == 0)
    // Not shared
    return NULL;

  for(uint32_t i = 0; i < *share_cnt; i++) {
    share_data *sd = &shared[i];
    if(sd->share_id == share_id)
      // Existing
      return sd;
  }

  // Not yet recorded, create a new record
  shared[*share_cnt] = (share_data){.share_id = share_id};

  return &shared[*share_cnt++];
}

uint8_t export_bin(uint8_t **bin, size_t *bin_sz, escene *scenes,
                   uint8_t scene_cnt)
{
  // Account for offset to where scene data starts + the number of scenes
  uint32_t size = 4 + 1;
  uint32_t shared_mesh_cnt = 0;

  // Calc required max size for all scenes. Can be smaller eventually due to
  // shared meshes.
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *s = &scenes[j];
    size += escene_calc_size(s);
    for(uint16_t i = 0; i < s->mesh_cnt; i++)
      if(s->meshes[i].share_id > 0)
        shared_mesh_cnt++;
  }

  *bin = malloc(size);

  // Reserve 4 bytes for offset to scene data
  uint8_t *p = *bin + sizeof(uint32_t);

  share_data shared[shared_mesh_cnt];
  uint32_t share_cnt = 0;

  // Write vertices (positions) if any
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->mesh_cnt; i++) {
      emesh *em = &es->meshes[i];
      if(em->type == OT_MESH) {
        share_data *sd = find_shared(em->share_id, shared, &share_cnt);
        if(!sd || sd->vertices_ofs == 0) {
          em->vertices_ofs = p - *bin;
          if(sd)
            sd->vertices_ofs = em->vertices_ofs;
#ifndef EXPORT_TRIS
          // Simply export array of vertices
          p = ie_write(p, em->vertices, em->vertex_cnt * sizeof(*em->vertices));
          logc("Wrote %i vertices starting at ofs: %i until ofs: %i for scene: "
               "%i and mesh: %i",
               em->vertex_cnt, em->vertices_ofs, p - *bin, j, i);
#else
          // Export vertices as tris
          for(uint16_t k = 0; k < em->index_cnt; k++)
            p = ie_write(p, &em->vertices[em->indices[k]],
                         sizeof(*em->vertices));
          logc("Wrote %i vertices starting at ofs: %i until ofs: %i for scene: "
               "%i and mesh: %i",
               em->index_cnt, em->vertices_ofs, p - *bin, j, i);
#endif
        } else
          em->vertices_ofs = sd->vertices_ofs;
      }
    }
  }

#ifdef EXPORT_NORMALS
  // Write normals if any
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->mesh_cnt; i++) {
      emesh *em = &es->meshes[i];
      if(em->type == OT_MESH) {
        share_data *sd = find_shared(em->share_id, shared, &share_cnt);
        if(!sd || sd->normals_ofs == 0) {
          em->normals_ofs = p - *bin;
          if(sd)
            sd->normals_ofs = em->normals_ofs;
          p = ie_write(p, em->normals, em->vertex_cnt * sizeof(*em->normals));
          logc("Wrote %i normals starting at ofs: %i until ofs: %i for scene: "
               "%i and mesh: %i",
               em->vertex_cnt, em->normals_ofs, p - *bin, j, i);
        } else
          em->normals_ofs = sd->normals_ofs;
      }
    }
  }
#endif

#ifndef EXPORT_TRIS
  // Write indices if any
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->mesh_cnt; i++) {
      emesh *em = &es->meshes[i];
      if(em->type == OT_MESH) {
        share_data *sd = find_shared(em->share_id, shared, &share_cnt);
        if(!sd || sd->indices_ofs == 0) {
          em->indices_ofs = p - *bin;
          if(sd)
            sd->indices_ofs = em->indices_ofs;
          p = ie_write(p, em->indices, em->index_cnt * sizeof(*em->indices));
          logc("Wrote %i indices starting at ofs: %i until ofs: %i for scene: "
               "%i and mesh: %i",
               em->index_cnt, em->indices_ofs, p - *bin, j, i);
        } else
          em->indices_ofs = sd->indices_ofs;
      }
    }
  }
#endif

  // Write offset to actual scene data at start of bin buffer (for initial
  // skipping of the mesh data during read)
  uint32_t ofs = p - *bin;
  ie_write(*bin, &ofs, sizeof(ofs));

  // Write scene cnt
  p = ie_write(p, &scene_cnt, sizeof(scene_cnt));
  logc("Wrote scene cnt: %i until ofs: %i", scene_cnt, p - *bin);

  // Write scene infos
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    p = ie_write(p, &es->mtl_cnt, sizeof(es->mtl_cnt));
    uint16_t cam_cnt = 0;
#ifdef EXPORT_CAMS
    cam_cnt = es->cam_cnt;
    p = ie_write(p, &cam_cnt, sizeof(es->cam_cnt));
#endif
    p = ie_write(p, &es->mesh_cnt, sizeof(es->mesh_cnt));
    p = ie_write(p, &es->inst_cnt, sizeof(es->inst_cnt));
    p = ie_write(p, &es->bg_col, sizeof(es->bg_col));
    logc(
        "Wrote scene data for scene %i until ofs: %i with %i mtls, %i cams, %i "
        "meshes %i mtls, %f/%f/%f bg col",
        j, p - *bin, es->mtl_cnt, cam_cnt, es->mesh_cnt, es->inst_cnt,
        es->bg_col.x, es->bg_col.y, es->bg_col.z);
  }

  // Write materials
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->mtl_cnt; i++) {
      mtl *m = &es->mtls[i];
      p = ie_write(p, &m->col, sizeof(m->col));
      p = ie_write(p, &m->metallic, sizeof(m->metallic));
      p = ie_write(p, &m->roughness, sizeof(m->roughness));
      p = ie_write(p, &m->ior, sizeof(m->ior));
      uint8_t flags =
          ((m->refractive > 0.0 ? 1 : 0) << 1) | (m->emissive > 0.0 ? 1 : 0);
      p = ie_write(p, &flags, sizeof(flags));
      logc("Wrote material %i of scene %i with until ofs: %i", i, j, p - *bin);
    }
  }

#ifdef EXPORT_CAMS
  // Write cameras
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->cam_cnt; i++) {
      p = ecam_write(p, &es->cams[i]);
      logc("Wrote cam %i of scene %i until ofs: %i", i, j, p - *bin);
    }
  }
#endif

  // Write meshes (primitive data)
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->mesh_cnt; i++) {
      p = emesh_write_primitive(p, &es->meshes[i]);
      logc("Wrote mesh %i of scene %i until ofs: %i", i, j, p - *bin);
    }
  }

  // Write instances
  for(uint8_t j = 0; j < scene_cnt; j++) {
    escene *es = &scenes[j];
    for(uint16_t i = 0; i < es->inst_cnt; i++) {
      p = einst_write(p, &es->instances[i]);
      logc("Wrote instance %i of scene %i until ofs: %i", i, j, p - *bin);
    }
  }

  *bin_sz = p - *bin;

  logc("Wrote %i bytes in total for %i scenes (calculated size: %i bytes)",
       *bin_sz, scene_cnt, size);

  if(*bin_sz > size) {
    logc("Something went wrong during writing. Written bytes should be less "
         "(shared meshes) or equal than calculated buffer size of %i bytes.",
         size);
    return 1;
  }

  return 0;
}
