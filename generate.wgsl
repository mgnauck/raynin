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

// General constants
const PI                  = 3.141592;

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

// State of rng seed
var<private> seed: vec4u;

// PCG 4D from Jarzynski/Olano: Hash Functions for GPU Rendering
fn rand4() -> vec4f
{
  seed = seed * 1664525u + 1013904223u;
  seed += seed.yzxy * seed.wxyz;
  seed = seed ^ (seed >> vec4u(16));
  seed += seed.yzxy * seed.wxyz;
  return ldexp(vec4f((seed >> vec4u(22)) ^ seed), vec4i(-32));
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

fn samplePixel(pixelPos: vec2f, r: vec2f) -> vec3f
{
  var pixelSample = globals.pixelTopLeft + globals.pixelDeltaX * pixelPos.x + globals.pixelDeltaY * pixelPos.y;
  pixelSample += (r.x - 0.5) * globals.pixelDeltaX + (r.y - 0.5) * globals.pixelDeltaY;
  return pixelSample;
}

fn sampleEye(r: vec2f) -> vec3f
{
  var eyeSample = globals.eye;
  if(globals.focAngle > 0) {
    let focRadius = globals.focDist * tan(0.5 * radians(globals.focAngle));
    let diskSample = sampleDisk(r);
    eyeSample += focRadius * (diskSample.x * globals.right + diskSample.y * globals.up);
  }
  return eyeSample;
}

@compute @workgroup_size(8, 8)
fn generate(@builtin(global_invocation_id) globalId: vec3u)
{
  if(any(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  seed = vec4u(globalId.xy, state.frame, state.gatheredSpp);

  let r0 = rand4();

  let gidx = globals.width * globalId.y + globalId.x;
  let bidx = (globals.width * globals.height * (state.cntIdx & 0x1)) + gidx;

  // Create primary ray
  let ori = sampleEye(r0.xy);
  let dir = normalize(samplePixel(vec2f(globalId.xy), r0.zw) - ori);
  rays[gidx].ori = ori;
  rays[gidx].dir = dir;

  // Initialize new path data
  pathData[bidx].seed = seed;
  pathData[bidx].throughput = vec3f(1.0);
  pathData[bidx].ori = ori;
  pathData[bidx].dir = dir;
  pathData[bidx].pidx = gidx << 8; // Bounce num is implicitly 0 (bits 0-3)
}
