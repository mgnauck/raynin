struct Global
{
  // Config
  width:        u32,
  height:       u32,
  maxBounces:   u32,
  tlasNodeOfs:  u32,
  bgColor:      vec3f,
  pad0:         f32,
  // Camera
  eye:          vec3f,
  vertFov:      f32,
  right:        vec3f,
  focDist:      f32,
  up:           vec3f,
  focAngle:     f32,
  // View
  pixelDeltaX:  vec3f,
  pad1:         f32,
  pixelDeltaY:  vec3f,
  pad2:         f32,
  pixelTopLeft: vec3f,
  pad3:         f32
}

struct Mtl
{
  col:          vec3f,          // Base color (diff col of non-metallics, spec col of metallics)
  metallic:     f32,            // Appearance range from dielectric to conductor (0 - 1)
  roughness:    f32,            // Perfect reflection to completely diffuse (0 - 1)
  ior:          f32,            // Index of refraction
  refractive:   f32,            // Flag if material refracts
  emissive:     f32             // Flag if material is emissive
}

struct Inst
{
  invTransform: mat3x4f,
  id:           u32,            // (mtl override id << 16) | (inst id & 0xffff)
  data:         u32,            // See comment on data in inst.h
  pad0:         u32,
  pad1:         u32
}

struct Node
{
  aabbMin:      vec3f,
  children:     u32,            // 2x 16 bits for left and right child
  aabbMax:      vec3f,
  idx:          u32             // Assigned on leaf nodes only
}

struct Tri
{
  v0:           vec3f,
  mtl:          u32,            // (mtl id & 0xffff)
  v1:           vec3f,
  ltriId:       u32,            // Set only if tri has light emitting material
  v2:           vec3f,
  pad0:         f32,
  n0:           vec3f,
  pad1:         f32,
  n1:           vec3f,
  pad2:         f32,
  n2:           vec3f,
  pad3:         f32
}

struct LTri
{
  v0:           vec3f,
  triId:        u32,            // Original tri id of the mesh (w/o inst data ofs)
  v1:           vec3f,
  pad0:         f32,
  v2:           vec3f,
  area:         f32,
  nrm:          vec3f,
  power:        f32,            // Precalculated product of area and emission
  emission:     vec3f,
  pad1:         f32
}

struct Ray
{
  ori:          vec3f,
  pad0:         f32,
  dir:          vec3f,
  pad1:         f32
}

struct ShadowRay
{
  ori:          vec3f,          // Shadow ray origin
  pidx:         u32,            // Pixel index where to deposit contribution
  dir:          vec3f,          // Position on the light (shadow ray target)
  dist:         f32,
  contribution: vec3f,
  pad0:         f32
}

struct PathData
{
  seed:         vec4u,          // Last rng seed used
  throughput:   vec3f,
  pdf:          f32,
  ori:          vec3f,
  pad0:         f32,
  dir:          vec3f,
  pidx:         u32             // Pixel idx in bits 8-31, bit 4-7 TBD, bounce num in bits 0-3
}

struct Hit
{
  t:            f32,
  u:            f32,
  v:            f32,
  e:            u32             // (tri id << 16) | (inst id & 0xffff)
}

struct State
{
  frame:        u32,
  gatheredSpp:  u32,
  rayCnt:       array<atomic<u32>, 2u>,
  shadowRayCnt: atomic<u32>,
  cntIdx:       u32,            // Index into current buf/counter (currently bit 0 only).
  hits:         array<Hit>      // TODO This is here because we have a limit of 8 storage buffers :(
}

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<uniform> materials: array<Mtl, 1024>; // One mtl per inst
@group(0) @binding(2) var<uniform> instances: array<Inst, 1024>; // Uniform buffer max is 64k bytes
@group(0) @binding(3) var<storage, read> tris: array<Tri>;
@group(0) @binding(4) var<storage, read> ltris: array<LTri>;
@group(0) @binding(5) var<storage, read> nodes: array<Node>;
@group(0) @binding(6) var<storage, read_write> rays: array<Ray>;
@group(0) @binding(7) var<storage, read_write> shadowRays: array<ShadowRay>;
@group(0) @binding(8) var<storage, read_write> pathData: array<PathData>;
@group(0) @binding(9) var<storage, read_write> accum: array<vec4f>;
@group(0) @binding(10) var<storage, read_write> state: State;

@vertex
fn quad(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  // Workaround for below code which does work with Firefox' naga
  switch(vertexIndex)
  {
    case 0u: {
      return vec4f(vec2f(-1.0, 1.0), 0.0, 1.0);
    }
    case 1u: {
      return vec4f(vec2f(-1.0), 0.0, 1.0);
    }
    case 2u: {
      return vec4f(vec2f(1.0), 0.0, 1.0);
    }
    default: {
      return vec4f(vec2f(1.0, -1.0), 0.0, 1.0);
    }
  }

  //let pos = array<vec2f, 4>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0));
  //return vec4f(pos[vertexIndex], 0.0, 1.0);
}

@fragment
fn blit(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let gidx = globals.width * u32(pos.y) + u32(pos.x);
  let col = vec4f(pow(accum[gidx].xyz / f32(state.gatheredSpp), vec3f(0.4545)), 1.0);
  accum[gidx] = vec4f(0.0, 0.0, 0.0, 1.0);
  return col;
}

@fragment
fn blitConverge(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let gidx = globals.width * u32(pos.y) + u32(pos.x);
  let col = vec4f(pow(accum[gidx].xyz / f32(state.gatheredSpp), vec3f(0.4545)), 1.0);
  return col;
}
