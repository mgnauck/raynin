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

// Scene data handling
const SHORT_MASK          = 0xffffu;
const SHAPE_TYPE_BIT      = 0x40000000u; // Bit 31
const INST_DATA_MASK      = 0x7fffffffu; // Bits 31-0 (includes shape type bit)
const MESH_SHAPE_MASK     = 0x3fffffffu; // Bits 30-0

// Shape types
const ST_PLANE            = 0u;
const ST_BOX              = 1u;
const ST_SPHERE           = 2u;

// General constants
const EPS                 = 0.001;
const INF                 = 3.402823466e+38;

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

// Traversal stacks
const MAX_NODE_CNT = 32u;
var<private> blasNodeStack: array<u32, MAX_NODE_CNT>;
var<private> tlasNodeStack: array<u32, MAX_NODE_CNT>;

fn minComp4(v: vec4f) -> f32
{
  return min(v.x, min(v.y, min(v.z, v.w)));
}

fn maxComp4(v: vec4f) -> f32
{
  return max(v.x, max(v.y, max(v.z, v.w)));
}

fn toMat4x4(m: mat3x4f) -> mat4x4f
{
  return mat4x4f(m[0], m[1], m[2], vec4f(0, 0, 0, 1));
}

// Laine et al. 2013; Afra et al. 2016: GPU efficient slabs test
// McGuire et al: A ray-box intersection algorithm and efficient dynamic voxel rendering
fn intersectAabb(ori: vec3f, invDir: vec3f, tfar: f32, minExt: vec3f, maxExt: vec3f) -> f32
{
  let t0 = (minExt - ori) * invDir;
  let t1 = (maxExt - ori) * invDir;

  let tmin = maxComp4(vec4f(min(t0, t1), EPS));
  let tmax = minComp4(vec4f(max(t1, t0), tfar));
  
  return select(INF, tmin, tmin <= tmax);
}

fn intersectAabbAnyHit(ori: vec3f, invDir: vec3f, tfar: f32, minExt: vec3f, maxExt: vec3f) -> bool
{
  let t0 = (minExt - ori) * invDir;
  let t1 = (maxExt - ori) * invDir;

  return maxComp4(vec4f(min(t0, t1), EPS)) <= minComp4(vec4f(max(t0, t1), tfar));
}

fn intersectPlaneAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let d = dir.y;
  if(abs(d) > EPS) {
    let t = -ori.y / d;
    return t < tfar && t > EPS;
  }
  return false;
}

fn intersectUnitSphereAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let a = dot(dir, dir);
  let b = dot(ori, dir);
  let c = dot(ori, ori) - 1.0;

  var d = b * b - a * c;
  if(d < 0.0) {
    return false;
  }

  d = sqrt(d);
  var t = (-b - d) / a;
  if(t <= EPS || tfar <= t) {
    t = (-b + d) / a;
    return t > EPS && t < tfar;
  }

  return true;
}

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f, u: ptr<function, f32>, v: ptr<function, f32>) -> f32
{
  // Vectors of two edges sharing vertex 0
  let edge1 = v1 - v0;
  let edge2 = v2 - v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < 0.0) {
    // Ray in plane of triangle
    return INF;
  }

  let invDet = 1 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  *u = dot(tvec, pvec) * invDet;
  if(*u < 0 || *u > 1) {
    return INF;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  *v = dot(dir, qvec) * invDet;
  if(*v < 0 || *u + *v > 1) {
    return INF;
  }

  // Calc distance
  return dot(edge2, qvec) * invDet;
}

fn intersectTriAnyHit(ori: vec3f, dir: vec3f, tfar: f32, tri: Tri) -> bool
{
  var u: f32;
  var v: f32;
  let dist = intersectTri(ori, dir, tri.v0, tri.v1, tri.v2, &u, &v);
  return dist > EPS && dist < tfar;
}

fn intersectBlasAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  let blasOfs = dataOfs << 1;

  // Early exit, unordered DF traversal
  loop {
    let node = &nodes[blasOfs + nodeIndex];
    if(intersectAabbAnyHit(ori, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeChildren = (*node).children;
      if(nodeChildren == 0) {
        // Leaf node, intersect contained triangle
        if(intersectTriAnyHit(ori, dir, tfar, tris[dataOfs + (*node).idx])) {
          return true;
        }
      } else {
        // Interior node, continue with left child
        nodeIndex = nodeChildren & SHORT_MASK;
        // Push right child node on stack
        blasNodeStack[nodeStackIndex] = nodeChildren >> 16;
        nodeStackIndex++;
        continue;
      }
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = blasNodeStack[nodeStackIndex];
    } else {
      return false;
    }
  }

  return false; // Required by firefox
}

fn intersectInstAnyHit(ori: vec3f, dir: vec3f, tfar: f32, inst: Inst) -> bool
{
  // Transform to object space of the instance
  let m = toMat4x4(inst.invTransform);
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = dir * mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);

  switch(inst.data & INST_DATA_MASK) {
    // Shape type
    case (SHAPE_TYPE_BIT | ST_PLANE): {
      return intersectPlaneAnyHit(oriObj, dirObj, tfar);
    }
    case (SHAPE_TYPE_BIT | ST_BOX): {
      return intersectAabbAnyHit(oriObj, 1.0 / dirObj, tfar, vec3f(-1), vec3f(1)); 
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      return intersectUnitSphereAnyHit(oriObj, dirObj, tfar);
    }
    default: {
      // Mesh type
      return intersectBlasAnyHit(oriObj, dirObj, 1.0 / dirObj, tfar, inst.data & MESH_SHAPE_MASK);
    }
  }
}

fn intersectTlasAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let invDir = 1.0 / dir;

  let tlasOfs = globals.tlasNodeOfs;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  // Early exit, unordered DF traversal
  loop {
    let node = &nodes[tlasOfs + nodeIndex];
    if(intersectAabbAnyHit(ori, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeChildren = (*node).children;
      if(nodeChildren == 0) {
        // Leaf node, intersect the single assigned instance
        if(intersectInstAnyHit(ori, dir, tfar, instances[(*node).idx])) {
          return true;
        }
      } else {
        // Interior node, continue traversal with left child node
        nodeIndex = nodeChildren & SHORT_MASK;
        // Push right child node on stack
        tlasNodeStack[nodeStackIndex] = nodeChildren >> 16;
        nodeStackIndex++;
        continue;
      }
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = tlasNodeStack[nodeStackIndex];
    } else {
      return false;
    }
  }

  return false; // Required by firefox
}

@compute @workgroup_size(8, 8)
fn traceShadowRay(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = globals.width * globalId.y + globalId.x;
  if(gidx >= atomicLoad(&state.shadowRayCnt)) {
    return;
  }

  let shadowRay = shadowRays[gidx];
  if(!intersectTlasAnyHit(shadowRay.ori, shadowRay.dir, shadowRay.dist)) {
    accum[shadowRay.pidx] += vec4f(shadowRay.contribution, 1.0);
  }
}
