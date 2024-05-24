struct Global
{
  width:        u32,
  height:       u32,
  spp:          u32,
  maxBounces:   u32,
  rngSeed1:     f32, // TODO Remove
  rngSeed2:     f32, // TODO Remove
  gatheredSpp:  f32, // TODO Remove/make u32
  weight:       f32,
  bgColor:      vec3f,
  frame:        f32, // TODO Make u32
  eye:          vec3f,
  vertFov:      f32,
  right:        vec3f,
  focDist:      f32,
  up:           vec3f,
  focAngle:     f32,
  pixelDeltaX:  vec3f,
  pad1:         f32,
  pixelDeltaY:  vec3f,
  pad2:         f32,
  pixelTopLeft: vec3f,
  pad3:         f32,
  // Use vec4 to adhere to stride requirement of 16 for arrays
  // TODO This should be possible as runtime sized array (not on Firefox yet?)
  randAlpha:    array<vec4f, 20u>
}

struct Hit
{
  t: f32,
  u: f32,
  v: f32,
  e: u32                  // (tri id << 16) | (inst id & 0xffff)
}

struct TlasNode
{
  aabbMin:  vec3f,
  children: u32,          // 2x 16 bits
  aabbMax:  vec3f,
  inst:     u32           // Assigned on leaf nodes only
}

struct BvhNode
{
  aabbMin:    vec3f,
  startIndex: u32,        // Either index of first object or left child node
  aabbMax:    vec3f,
  objCount:   u32
}

struct Mtl
{
  col: vec3f,             // Base color (diff col of non-metallics, spec col of metallics)
  metallic: f32,          // Appearance range from dielectric to conductor (0 - 1)
  roughness: f32,         // Perfect reflection to completely diffuse (0 - 1)
  reflectance: f32,       // Reflectance of non-metallic materials (0 - 1)
  pad0: f32,
  pad1: f32
}

struct IA
{
  pos:      vec3f,
  dist:     f32,
  nrm:      vec3f,
  faceDir:  f32,
  wo:       vec3f,
  ltriId:   u32
}

struct Tri
{
  v0:     vec3f,
  mtl:    u32,            // (mtl id & 0xffff)
  v1:     vec3f,
  ltriId: u32,            // Set only if tri has light emitting material
  v2:     vec3f,
  pad0:   f32,
  n0:     vec3f,
  pad1:   f32,
  n1:     vec3f,
  pad2:   f32,
  n2:     vec3f,
  pad3:   f32,
//#ifdef TEXTURE_SUPPORT
/*uv0:    vec2f,
  uv1:    vec2f,
  uv2:    vec2f,
  pad4    f32,
  pad5:   f32*/
//#endif
}

struct LTri
{
  v0:       vec3f,
  instId:   u32,
  v1:       vec3f,
  triId:    u32,          // Original tri id of the mesh (w/o inst data ofs)
  v2:       vec3f,
  area:     f32,
  nrm:      vec3f,
  power:    f32,          // Precalculated product of area and emission
  emission: vec3f,
  pad0:     f32
}

struct Inst
{
  transform:    mat3x4f,
  aabbMin:      vec3f,
  id:           u32,      // (mtl override id << 16) | (inst id & 0xffff)
  invTransform: mat3x4f,
  aabbMax:      vec3f,
  data:         u32       // See comment on data in inst.h
}

// Scene data bit handling
const INST_ID_MASK        = 0xffffu;
const MTL_ID_MASK         = 0xffffu;
const TLAS_NODE_MASK      = 0xffffu;

const SHAPE_TYPE_BIT      = 0x40000000u;
const MESH_SHAPE_MASK     = 0x3fffffffu;
const MTL_OVERRIDE_BIT    = 0x80000000u;
const INST_DATA_MASK      = 0x7fffffffu;

// Shape types
const ST_PLANE            = 0u;
const ST_BOX              = 1u;
const ST_SPHERE           = 2u;

// General constants
const EPS                 = 0.0001;
const GEOM_EPS            = 0.0001;
const INF                 = 3.402823466e+38;
const PI                  = 3.141592;
const TWO_PI              = 6.283185;
const INV_PI              = 1 / PI;
const ORIGIN              = vec3f(1 / 32.0); // posOfs()
const FLOAT_SCALE         = 1 / 65536.0;
const INT_SCALE           = 256.0;

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<storage, read> tris: array<Tri>;
@group(0) @binding(2) var<storage, read> ltris: array<LTri>;
@group(0) @binding(3) var<storage, read> indices: array<u32>;
@group(0) @binding(4) var<storage, read> bvhNodes: array<BvhNode>;
@group(0) @binding(5) var<storage, read> tlasNodes: array<TlasNode>;
@group(0) @binding(6) var<storage, read> instances: array<Inst>;
@group(0) @binding(7) var<storage, read> materials: array<Mtl>;
@group(0) @binding(8) var<storage, read_write> buffer: array<vec4f>;

const MAX_LTRIS     = 64;
const MAX_NODE_CNT  = 32;

// Traversal stacks for bvhs
var<private> bvhNodeStack: array<u32, MAX_NODE_CNT>;
var<private> tlasNodeStack: array<u32, MAX_NODE_CNT>;

// State of prng
var<private> rng: vec4u;

fn minComp3(v: vec3f) -> f32
{
  return min(v.x, min(v.y, v.z));
}

fn minComp4(v: vec4f) -> f32
{
  return min(v.x, min(v.y, min(v.z, v.w)));
}

fn maxComp3(v: vec3f) -> f32
{
  return max(v.x, max(v.y, v.z));
}

fn maxComp4(v: vec4f) -> f32
{
  return max(v.x, max(v.y, max(v.z, v.w)));
}

fn toMat3x3(m: mat3x4f) -> mat3x3f
{
  return mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);
}

fn toMat4x4(m: mat3x4f) -> mat4x4f
{
  return mat4x4f(m[0], m[1], m[2], vec4f(0, 0, 0, 1));
}

// Duff et al: Building an Orthonormal Basis, Revisited
fn createONB(n: vec3f) -> mat3x3f
{
  var r: mat3x3f;
  let s = select(-1.0, 1.0, n.z >= 0.0); // = copysign
  let a = -1.0 / (s + n.z);
  let b = n.x * n.y * a;
  r[0] = vec3f(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
  r[1] = n;
  r[2] = vec3f(b, s + n.y * n.y * a, -n.y);
  return r;
}

// Waechter/Binder: A Fast and Robust Method for Avoiding Self-Intersection
// From Ray Tracing Gems (chapter 6). Hope the vectorization & conversion to wgsl is correct :)
fn posOfs(p: vec3f, n: vec3f) -> vec3f
{
  let ofInt = vec3i(INT_SCALE * n);
  let pInt = bitcast<vec3f>(bitcast<vec3i>(p) + select(ofInt, -ofInt, p < vec3f(0)));
  return select(pInt, p + FLOAT_SCALE * n, abs(p) < ORIGIN);
}

// PCG 4D from Jarzynski/Olano: Hash Functions for GPU Rendering
fn rand4() -> vec4f
{
  rng = rng * 1664525u + 1013904223u;

  rng.x += rng.y * rng.w;
  rng.y += rng.z * rng.x;
  rng.z += rng.x * rng.y;
  rng.w += rng.y * rng.z;

  rng = rng ^ (rng >> vec4u(16));
  rng.x += rng.y * rng.w;
  rng.y += rng.z * rng.x;
  rng.z += rng.x * rng.y;
  rng.w += rng.y * rng.z;

  return ldexp(vec4f((rng >> vec4u(22)) ^ rng), vec4i(-32));
}

// Parallelogram method, https://mathworld.wolfram.com/TrianglePointPicking.html
fn sampleBarycentric(r: vec2f) -> vec3f
{
  if(r.x + r.y > 1.0) {
    return vec3f(1.0 - r.x, 1.0 - r.y, -1.0 + r.x + r.y);
  }

  return vec3f(r, 1 - r.x - r.y);
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

// https://mathworld.wolfram.com/SpherePointPicking.html
fn sampleUnitSphere(r: vec2f) -> vec3f
{
  let u = 2 * r.x - 1;
  let theta = 2 * PI * r.y;
  let radius = sqrt(1 - u * u);
  return vec3f(radius * cos(theta), radius * sin(theta), u);
}

fn sampleHemisphere(nrm: vec3f, r: vec2f) -> vec3f
{
  // Sample uniform and reject directions in wrong hemisphere
  let v = sampleUnitSphere(r);
  return select(-v, v, dot(v, nrm) > 0);
}

fn sampleHemisphereCos(r: vec2f) -> vec3f
{
  // Sample disc at base of hemisphere uniformly
  let disk = sampleDisk(r);

  // Project samples up to the hemisphere
  return vec3f(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), disk.y);
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

fn intersectPlane(ori: vec3f, dir: vec3f, instId: u32, h: ptr<function, Hit>) -> bool
{
  let d = dir.y;
  if(abs(d) > EPS) {
    let t = -ori.y / d;
    if(t < (*h).t && t > EPS) {
      (*h).t = t;
      (*h).e = (ST_PLANE << 16) | (instId & INST_ID_MASK);
      return true;
    }
  }
  return false;
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

fn intersectUnitBox(ori: vec3f, invDir: vec3f, instId: u32, h: ptr<function, Hit>) -> bool
{
  let t0 = (vec3f(-1.0) - ori) * invDir;
  let t1 = (vec3f( 1.0) - ori) * invDir;

  let tmin = maxComp3(min(t0, t1));
  let tmax = minComp3(max(t0, t1));
 
  if(tmin <= tmax) {
    if(tmin < (*h).t && tmin > EPS) {
      (*h).t = tmin;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
      return true;
    }
    if(tmax < (*h).t && tmax > EPS) {
      (*h).t = tmax;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
      return true;
    }
  }
  return false;
}

fn intersectUnitSphere(ori: vec3f, dir: vec3f, instId: u32, h: ptr<function, Hit>) -> bool
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
  if(t <= EPS || (*h).t <= t) {
    t = (-b + d) / a;
    if(t <= EPS || (*h).t <= t) {
      return false;
    }
  }

  (*h).t = t;
  (*h).e = (ST_SPHERE << 16) | (instId & INST_ID_MASK);
  return true;
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

  if(abs(det) < EPS) {
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

fn intersectTriClosest(ori: vec3f, dir: vec3f, tri: Tri, instTriId: u32, h: ptr<function, Hit>) -> bool
{
  var u: f32;
  var v: f32;
  let dist = intersectTri(ori, dir, tri.v0, tri.v1, tri.v2, &u, &v);
  if(dist > EPS && dist < (*h).t) {
    (*h).t = dist;
    (*h).u = u;
    (*h).v = v;
    (*h).e = instTriId;
    return true;
  }

  return false;
}

fn intersectTriAnyHit(ori: vec3f, dir: vec3f, tfar: f32, tri: Tri) -> bool
{
  var u: f32;
  var v: f32;
  let dist = intersectTri(ori, dir, tri.v0, tri.v1, tri.v2, &u, &v);
  return dist > EPS && dist < tfar;
}

fn intersectBvh(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, Hit>)
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  let bvhOfs = dataOfs << 1;

  // Sorted DF traversal (near child first + skip far child if not within current dist)
  loop {
    let node = &bvhNodes[bvhOfs + nodeIndex];
    let nodeStartIndex = (*node).startIndex;
    let nodeObjCount = (*node).objCount;
   
    if(nodeObjCount != 0) {
      // Leaf node, intersect all contained triangles
      for(var i=0u; i<nodeObjCount; i++) {
        let triId = indices[dataOfs + nodeStartIndex + i];
        intersectTriClosest(ori, dir, tris[dataOfs + triId], (triId << 16) | (instId & INST_ID_MASK), hit);
      }
    } else {
      // Interior node
      var leftChildIndex = nodeStartIndex;
      var rightChildIndex = nodeStartIndex + 1;

      let leftChildNode = &bvhNodes[bvhOfs + leftChildIndex];
      let rightChildNode = &bvhNodes[bvhOfs + rightChildIndex];

      // Intersect both child node aabbs
      var leftDist = intersectAabb(ori, invDir, (*hit).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      var rightDist = intersectAabb(ori, invDir, (*hit).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
      // Swap for nearer child
      if(leftDist > rightDist) {
        let td = rightDist;
        rightDist = leftDist;
        leftDist = td;

        leftChildIndex += 1u;
        rightChildIndex -= 1u;
      }

      if(leftDist < INF) {
        // Continue with nearer child node
        nodeIndex = leftChildIndex;
        if(rightDist < INF) {
          // Push farther child on stack if also a hit
          bvhNodeStack[nodeStackIndex] = rightChildIndex;
          nodeStackIndex++;
        }
        continue;
      }
      // Missed both child nodes
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = bvhNodeStack[nodeStackIndex];
    } else {
      return;
    }
  }
}

fn intersectBvhAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  let bvhOfs = dataOfs << 1;

  // Early exit, unordered DF traversal
  loop {
    let node = &bvhNodes[bvhOfs + nodeIndex];
    if(intersectAabbAnyHit(ori, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeStartIndex = (*node).startIndex;
      let nodeObjCount = (*node).objCount; 
      if(nodeObjCount != 0) {
        // Leaf node, intersect all contained triangles
        for(var i=0u; i<nodeObjCount; i++) {
          let triId = indices[dataOfs + nodeStartIndex + i];
          if(intersectTriAnyHit(ori, dir, tfar, tris[dataOfs + triId])) {
            return true;
          }
        }
      } else {
        // Interior node, continue with left child
        nodeIndex = nodeStartIndex;
        // Push right child node on stack
        bvhNodeStack[nodeStackIndex] = nodeStartIndex + 1;
        nodeStackIndex++;
        continue;
      }
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = bvhNodeStack[nodeStackIndex];
    } else {
      return false;
    }
  }

  return false; // Required by firefox
}

fn intersectInst(ori: vec3f, dir: vec3f, inst: Inst, hit: ptr<function, Hit>)
{
  // Transform to object space of the instance
  let m = toMat4x4(inst.invTransform);
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = dir * mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);

  switch(inst.data & INST_DATA_MASK) {
    // Shape type
    case (SHAPE_TYPE_BIT | ST_PLANE): {
      intersectPlane(oriObj, dirObj, inst.id, hit);
    }
    case (SHAPE_TYPE_BIT | ST_BOX): {
      intersectUnitBox(oriObj, 1 / dirObj, inst.id, hit);
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      intersectUnitSphere(oriObj, dirObj, inst.id, hit);
    }
    default: {
      // Mesh type
      intersectBvh(oriObj, dirObj, 1 / dirObj, inst.id, inst.data & MESH_SHAPE_MASK, hit);
    }
  }
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
      return intersectAabbAnyHit(oriObj, 1 / dirObj, tfar, vec3f(-1), vec3f(1)); 
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      return intersectUnitSphereAnyHit(oriObj, dirObj, tfar);
    }
    default: {
      // Mesh type
      return intersectBvhAnyHit(oriObj, dirObj, 1 / dirObj, tfar, inst.data & MESH_SHAPE_MASK);
    }
  }
}

fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> Hit
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  let invDir = 1 / dir;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal (near child first + skip far child if not within current dist)
  loop {
    let node = &tlasNodes[nodeIndex];
    let nodeChildren = (*node).children;

    if(nodeChildren == 0) {
      // Leaf node, intersect the single assigned instance 
      intersectInst(ori, dir, instances[(*node).inst], &hit);
    } else {
      // Interior node
      var leftChildIndex = nodeChildren & TLAS_NODE_MASK;
      var rightChildIndex = nodeChildren >> 16;

      let leftChildNode = &tlasNodes[leftChildIndex];
      let rightChildNode = &tlasNodes[rightChildIndex];

      // Intersect both child node aabbs
      var leftDist = intersectAabb(ori, invDir, hit.t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      var rightDist = intersectAabb(ori, invDir, hit.t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

      // Swap for nearer child
      if(leftDist > rightDist) {
        let td = rightDist;
        rightDist = leftDist;
        leftDist = td;

        let ti = rightChildIndex;
        rightChildIndex = leftChildIndex;
        leftChildIndex = ti;
      }

      if(leftDist < INF) {
        // Continue with nearer child node
        nodeIndex = leftChildIndex;
        if(rightDist < INF) {
          // Push farther child on stack if also a hit
          tlasNodeStack[nodeStackIndex] = rightChildIndex;
          nodeStackIndex++;
        }
        continue;
      }
      // Missed both child nodes
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = tlasNodeStack[nodeStackIndex];
    } else {
      return hit;
    }
  }

  return hit; // Required by firefox
}

fn intersectTlasAnyHit(p0: vec3f, p1: vec3f) -> bool
{
  var dir = p1 - p0;
  let tfar = length(dir);
  dir /= tfar;
  let invDir = 1 / dir;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  // Early exit, unordered DF traversal
  loop {
    let node = &tlasNodes[nodeIndex];
    if(intersectAabbAnyHit(p0, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeChildren = (*node).children;
      if(nodeChildren == 0) {
        // Leaf node, intersect the single assigned instance
        if(intersectInstAnyHit(p0, dir, tfar, instances[(*node).inst])) {
          return true;
        }
      } else {
        // Interior node, continue traversal with left child node
        nodeIndex = nodeChildren & TLAS_NODE_MASK;
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

fn calcShapeNormal(inst: Inst, hitPos: vec3f) -> vec3f
{
  switch(inst.data & MESH_SHAPE_MASK) {
    case ST_BOX: {
      let pos = (vec4f(hitPos, 1.0) * toMat4x4(inst.invTransform)).xyz;
      var nrm = pos * step(vec3f(1.0) - abs(pos), vec3f(EPS));
      return normalize(nrm * toMat3x3(inst.transform));
    }
    case ST_PLANE: {
      return normalize(vec3f(0.0, 1.0, 0.0) * toMat3x3(inst.transform));
    }
    case ST_SPHERE: {
      let m = inst.transform;
      return normalize(hitPos - vec3f(m[0][3], m[1][3], m[2][3]));
    }
    default: {
      // Error
      return vec3f(100);
    }
  }
}

fn calcTriNormal(h: Hit, inst: Inst, tri: Tri) -> vec3f
{
  let nrm = tri.n1 * h.u + tri.n2 * h.v + tri.n0 * (1.0 - h.u - h.v);
  return normalize(nrm * toMat3x3(inst.transform));
}

fn isEmissive(mtl: Mtl) -> bool
{
  return any(mtl.col > vec3f(1.0));
}

fn luminance(col: vec3f) -> f32
{
	return dot(col, vec3f(0.2126, 0.7152, 0.0722));
}

fn mtlToSpecularF0(mtl: Mtl) -> vec3f
{
  // For metallic materials we use the base color, otherwise the ior
  //var f0 = vec3f(abs((1.0 - mtl.ior) / (1.0 + mtl.ior)));
  //return mix(f0 * f0, mtl.col, mtl.metallic);

  // For metallic materials we use the base color, otherwise the reflectance
  var f0 = vec3f(0.16 * mtl.reflectance * mtl.reflectance);
  return mix(f0, mtl.col, mtl.metallic);
}

fn getRoughness(mtl: Mtl) -> f32
{
  return saturate(mtl.roughness * mtl.roughness - EPS) + EPS;
}

// Boksansky: Crash Cource in BRDF Implementation
// Cook/Torrance: A Reflectance Model for Computer Graphics
// Walter et al: Microfacet Models for Refraction through Rough Surfaces
fn distributionGGX(n: vec3f, h: vec3f, alpha: f32) -> f32
{
  let NoH = dot(n, h);
  let alphaSqr = alpha * alpha;
  let NoHSqr = NoH * NoH;
  let den = NoHSqr * alphaSqr + (1.0 - NoHSqr);
  return (step(EPS, NoH) * alphaSqr) / (PI * den * den);
}

fn fresnelSchlick(cosTheta: f32, f0: vec3f) -> vec3f
{
  return f0 + (vec3f(1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

fn geometryPartialGGX(v: vec3f, n: vec3f, h: vec3f, alpha: f32) -> f32
{
  var VoHSqr = max(0.0, dot(v, h));
  let chi = step(EPS, VoHSqr / max(0.0, dot(n, v)));
  VoHSqr = VoHSqr * VoHSqr;
  let tan2 = (1.0 - VoHSqr) / VoHSqr;
  return (chi * 2.0) / (1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

fn sampleGGX(roughness: f32, r0: vec2f) -> vec3f
{
  // TODO Optimize atan usage to cos(theta)
  let theta = atan(roughness * sqrt(r0.x / (1.0 - r0.x)));
  let phi = TWO_PI * r0.y;
  let sinT = sin(theta);
  return normalize(vec3f(cos(phi) * sinT, cos(theta), sin(phi) * sinT));
}

fn sampleSpecular(mtl: Mtl, wo: vec3f, n: vec3f, r0: vec2f, wi: ptr<function, vec3f>) -> f32
{
  let r = reflect(-wo, n);
  *wi = normalize(createONB(r) * sampleGGX(getRoughness(mtl), r0));

  if(dot(n, wo) <= 0.0 || dot(n, *wi) <= 0.0) {
    return 0.0;
  }

  return sampleSpecularPdf(mtl, wo, n, *wi);
}

fn sampleSpecularPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  let h = normalize(wo + wi);
  return distributionGGX(n, h, getRoughness(mtl)) * max(0.0, dot(n, h));
}

fn evalSpecular(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: ptr<function, vec3f>) -> vec3f
{
  let h = normalize(wo + wi);

  let cosT = max(0.0, dot(n, wi));
  let sinT = sqrt(1.0 - cosT * cosT);

  let roughness = getRoughness(mtl);
  let geom = geometryPartialGGX(wo, n, h, roughness) * geometryPartialGGX(wi, n, h, roughness); 
  
  let f0 = mtlToSpecularF0(mtl);
  *fres = fresnelSchlick(max(0.0, dot(wo, h)), f0);

  let denom = saturate(4.0 * (max(0.0, dot(n, wo)) * max(0.0, dot(n, h)) + 0.05));

  // Pre-divided by pdf
  return geom * (*fres) * sinT / denom;
}

fn sampleDiffuse(n: vec3f, r0: vec2f, wi: ptr<function, vec3f>) -> f32
{
  *wi = createONB(n) * sampleHemisphereCos(r0);
  return max(0.0, dot(n, *wi)) * INV_PI;
}

fn sampleDiffusePdf(n: vec3f, wi: vec3f) -> f32
{
  return max(0.0, dot(n, wi)) * INV_PI;
}

fn evalDiffuse(mtl: Mtl, n: vec3f, wi: vec3f) -> vec3f
{
  return (1.0 - mtl.metallic) * mtl.col * INV_PI;
}

fn getSpecularProb(mtl: Mtl, wo: vec3f, n: vec3f) -> f32
{
  // No half vector available yet, use normal for fresnel diff/spec separation
  let f0 = mtlToSpecularF0(mtl);
  let fres = fresnelSchlick(max(0.0, dot(n, wo)), f0);

  let s = luminance(fres);
  let d = (1.0 - s) * luminance((1.0 - mtl.metallic) * mtl.col);

  return clamp(s / max(EPS, s + d), 0.1, 0.9);
}

fn sampleMaterial(mtl: Mtl, wo: vec3f, n: vec3f, r0: vec3f, wi: ptr<function, vec3f>, isSpecular: ptr<function, bool>, specProb: ptr<function, f32>, pdf: ptr<function, f32>) -> bool
{
  *specProb = getSpecularProb(mtl, wo, n);
  if(r0.z < *specProb) {
    *isSpecular = true;
    *pdf = sampleSpecular(mtl, wo, n, r0.xy, wi);
  } else {
    *isSpecular = false;
    *pdf = sampleDiffuse(n, r0.xy, wi);
  }

  return *pdf >= EPS;
}

fn evalMaterial(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, isSpecular: bool, specProb: f32) -> vec3f
{
  if(isSpecular) {
    var fres: vec3f;
    // We want to apply rendering equation's dot(n, wi) elsewhere, so remove it here
    return evalSpecular(mtl, wo, n, wi, &fres) / (specProb * max(0.0, dot(n, wi)));
  } else {
    return evalDiffuse(mtl, n, wi) / ((1.0 - specProb) * dot(n, wi) * INV_PI);
  }
}

fn evalMaterialCombined(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> vec3f
{
  if(dot(n, wo) <= 0.0 || dot(n, wi) <= 0.0) {
    return vec3f(0);
  }

  var fresnel: vec3f;
  // TODO Improve explicit undo of applied pdf for specular
  return evalSpecular(mtl, wo, n, wi, &fresnel) * sampleSpecularPdf(mtl, wo, n, wi) + (vec3f(1) - fresnel) * evalDiffuse(mtl, n, wi);
}

fn sampleLights(pos: vec3f, n: vec3f, r0: vec3f, ltriPos: ptr<function, vec3f>, ltriNrm: ptr<function, vec3f>, emission: ptr<function, vec3f>, pdf: ptr<function, f32>) -> bool
{
  // Pick a ltri uniformly
  let ltriCnt = arrayLength(&ltris);
  let ltriId = u32(floor(r0.z * f32(ltriCnt)));
  let ltri = &ltris[ltriId];

  let bc = sampleBarycentric(r0.xy);
  *ltriPos = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;
  *ltriNrm = (*ltri).nrm;

  let ldir = pos - *ltriPos;
  var visible = dot(ldir, *ltriNrm) > 0; // Front side of ltri only
  visible &= dot(ldir, n) < 0; // Not facing (behind)

  *pdf = 1.0 / ((*ltri).area * f32(ltriCnt));
  *emission = (*ltri).emission;

  return visible && *pdf > EPS;
}

fn sampleLightsPdf(pos: vec3f, n: vec3f, ltriId: u32) -> f32
{
  return 1.0 / (ltris[ltriId].area * f32(arrayLength(&ltris)));
}

fn geomSolidAngle(pos: vec3f, surfPos: vec3f, surfNrm: vec3f) -> f32
{
  // Distance attenuation ('geometric solid angle')
  var dir = pos - surfPos;
  let dist = length(dir);
  dir /= dist;
  return abs(dot(surfNrm, dir)) / (dist * dist);
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

fn calcLTriContribution(pos: vec3f, nrm: vec3f, ltriPoint: vec3f, ltriNrm: vec3f, lightPower: f32) -> f32
{
  var lightDir = ltriPoint - pos;
  let invDist = 1.0 / length(lightDir);
  lightDir *= invDist;

  // Inclination between light normal and light dir
  // (how much the light faces the light dir)
  let lnDotL = max(0.0, -dot(ltriNrm, lightDir));

  // Inclination between surface normal and light dir
  // (how much the surface aligns with the light dir or if the light is behind)
  let nDotL = max(0.0, dot(nrm, lightDir));

  // Inclination angles scale our measure of light power / dist^2
  return lnDotL * nDotL * lightPower * invDist * invDist;
}

fn calcLTriPickProb(pos: vec3f, nrm: vec3f, ltriPoint: vec3f, ltriId: u32) -> f32
{
  // Calculate picking probability with respect to ltri contributions
  var contributions: array<f32, MAX_LTRIS>;
  var totalContrib = 0.0;
  let ltriCnt = arrayLength(&ltris);

  // Calc contribution of each ltri with the given light dir/pos and total of all contributions
  for(var i=0u; i<ltriCnt; i++) {
    let ltri = &ltris[i];
    let curr = calcLTriContribution(pos, nrm, ltriPoint, (*ltri).nrm, (*ltri).power);
    contributions[i] = curr;
    totalContrib += curr;
  }

  // Scale contribution by total, so that our picking pdf integrates to 1
  return select(0.0, contributions[ltriId] / totalContrib, totalContrib > EPS);
}

// https://computergraphics.stackexchange.com/questions/4792/path-tracing-with-multiple-lights/
fn pickLTriRandomly(pos: vec3f, nrm: vec3f, r: f32, bc: vec3f, pdf: ptr<function, f32>) -> u32
{
  // Calculate picking probability with respect to ltri contributions
  var contributions: array<f32, MAX_LTRIS>;
  var totalContrib = 0.0;
  let ltriCnt = arrayLength(&ltris);

  // Calc contribution of each ltri and total of all contributions
  for(var i=0u; i<ltriCnt; i++) {
    let ltri = &ltris[i];
    let ltriPoint = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;
    let curr = calcLTriContribution(pos, nrm, ltriPoint, (*ltri).nrm, (*ltri).power);
    contributions[i] = curr;
    totalContrib += curr;
  }

  // No ltri can be picked, i.e. each ltri faces away
  if(totalContrib < EPS) {
    *pdf = 0.0;
    return 0u;
  }

   // Same as scaling contributions[i] by totalContrib
  let rScaled = r * totalContrib;

  // Randomly pick the ltri according to the CDF
  // CDF is pdf for value X or all smaller ones
  var cumulative = 0.0;
  var ltriId: u32;
  for(var i=0u; i<ltriCnt; i++) {
    cumulative += contributions[i];
    if(rScaled <= cumulative) {
      ltriId = i;
      break;
    }
  }

  // Scale contribution by total, so that our picking pdf integrates to 1
  *pdf = contributions[ltriId] / totalContrib;
  return ltriId;
}

fn finalizeHit(ori: vec3f, dir: vec3f, hit: Hit, ia: ptr<function, IA>, mtl: ptr<function, Mtl>)
{
  let inst = instances[hit.e & INST_ID_MASK];

  (*ia).dist = hit.t;
  (*ia).pos = ori + hit.t * dir;
  (*ia).wo = -dir;
 
  if((inst.data & SHAPE_TYPE_BIT) > 0) {
    // Shape
    *mtl = materials[(inst.id >> 16) & MTL_ID_MASK];
    (*ia).nrm = calcShapeNormal(inst, (*ia).pos);
  } else {
    // Mesh
    let ofs = inst.data & MESH_SHAPE_MASK;
    let tri = tris[ofs + (hit.e >> 16)];
    // Either use the material id from the triangle or the material override from the instance
    *mtl = materials[select(tri.mtl, inst.id >> 16, (inst.data & MTL_OVERRIDE_BIT) > 0) & MTL_ID_MASK];
    (*ia).nrm = calcTriNormal(hit, inst, tri);
    (*ia).ltriId = tri.ltriId;
  }

  // Backside hit
  (*ia).faceDir = select(1.0, -1.0, dot(dir, (*ia).nrm) > 0);
}

fn renderMIS(oriPrim: vec3f, dirPrim: vec3f) -> vec3f
{
  // Primary ray
  var hit = intersectTlas(oriPrim, dirPrim, INF);
  if(hit.t == INF) {
    return globals.bgColor;
  }

  var ia: IA;
  var mtl: Mtl;
  finalizeHit(oriPrim, dirPrim, hit, &ia, &mtl);

  if(isEmissive(mtl)) {
    return mtl.col * step(0.0, ia.faceDir);
  }

  var col = vec3f(0);
  var throughput = vec3f(1);
  var wasSpecular = false;

  /*
  TODO:
  - Optimize GGX sampling (direct cos(theta))
  - Refactoring PDF handling for Cook-Torrance
  - Non-uniform light picking back in
  - Transmission/Refraction?
  - Path space regularization (increase roughness)
  - Try LDS once more
  - Clean up globals
  */

  for(var bounces=0u; bounces<globals.maxBounces; bounces++) {

    let r0 = rand4();
    let r1 = rand4();

    // Sample lights directly (NEE)
    var ltriPos: vec3f;
    var ltriNrm: vec3f;
    var emission: vec3f;
    var ltriPdf: f32;
    if(sampleLights(ia.pos, ia.faceDir * ia.nrm, r0.xyz, &ltriPos, &ltriNrm, &emission, &ltriPdf)) {
      // Apply MIS
      // Veach/Guibas: Optimally Combining Sampling Techniques for Monte Carlo Rendering
      var ltriWi = normalize(ltriPos - ia.pos);
      let gsa = geomSolidAngle(ia.pos, ltriPos, ltriNrm);
      let diffusePdf = sampleDiffusePdf(ia.faceDir * ia.nrm, ltriWi);
      let specularPdf = sampleSpecularPdf(mtl, ia.wo, ia.faceDir * ia.nrm, ltriWi);
      let weight = ltriPdf / (ltriPdf + specularPdf * gsa + diffusePdf * gsa);
      let brdf = evalMaterialCombined(mtl, ia.wo, ia.faceDir * ia.nrm, ltriWi);
      if(any(brdf > vec3f(0)) && !intersectTlasAnyHit(posOfs(ia.pos, ia.faceDir * ia.nrm), posOfs(ltriPos, ltriNrm))) {
        col += throughput * brdf * gsa * weight * emission * max(0.0, dot(ia.faceDir * ia.nrm, ltriWi)) / ltriPdf;
      }
    }

    // Sample material
    var wi: vec3f;
    var specProb: f32;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, r1.xyz, &wi, &wasSpecular, &specProb, &pdf)) {
      break;
    }

    // Trace indirect light direction
    let ori = posOfs(ia.pos, ia.faceDir * ia.nrm);
    let dir = wi;
    hit = intersectTlas(ori, dir, INF);
    if(hit.t == INF) {
      col += throughput * globals.bgColor;
      break;
    }
    
    // Scale indirect light contribution by material
    throughput *= evalMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, wi, wasSpecular, specProb) * dot(ia.nrm, wi); // ia.faceDir * ia.nrm

    // Save for light hit MIS calculation
    let lastPos = ia.pos;
    let lastNrm = ia.faceDir * ia.nrm;

    finalizeHit(ori, dir, hit, &ia, &mtl);

    // Hit light via material direction
    if(isEmissive(mtl)) {
      // Lights emit from front side only
      if(ia.faceDir > 0) {
        // Apply MIS
        let gsa = geomSolidAngle(lastPos, ia.pos, ia.nrm); // = ltri pos and ltri nrm
        let ltriPdf = sampleLightsPdf(lastPos, lastNrm, ia.ltriId);
        let weight = pdf * gsa / (pdf * gsa + ltriPdf);
        col += throughput * weight * mtl.col;
      }
      break;
    }

    // Russian roulette
    // Terminate with prob inverse to throughput
    let p = min(0.95, maxComp3(throughput));
    if(r1.w > p) {
      break;
    }
    // Account for bias introduced by path termination (pdf = p)
    // Boost surviving paths by their probability to be terminated
    throughput *= 1.0 / p;
  }

  return col;
}

fn renderNEE(oriPrim: vec3f, dirPrim: vec3f) -> vec3f
{
  var col = vec3f(0);
  var throughput = vec3f(1);
  var ori = oriPrim;
  var dir = dirPrim;
  var wasSpecular = false;

  for(var bounces=0u; bounces<globals.maxBounces; bounces++) {

    var hit = intersectTlas(ori, dir, INF);
    if(hit.t == INF) {
      col += throughput * globals.bgColor;
      break;
    }

    var ia: IA;
    var mtl: Mtl;
    finalizeHit(ori, dir, hit, &ia, &mtl);

    // Hit a light
    if(isEmissive(mtl)) {
      // Lights emit from front side only
      if(ia.faceDir > 0) {
        // Last bounce was specular
        if(bounces == 0 || wasSpecular) {
          col += throughput * mtl.col;
        }
      }
      break;
    }

    let r0 = rand4();
    let r1 = rand4();

    // Sample lights directly (NEE)
    var ltriPos: vec3f;
    var ltriNrm: vec3f;
    var emission: vec3f;
    var ltriPdf: f32;
    if(sampleLights(ia.pos, ia.faceDir * ia.nrm, r0.xyz, &ltriPos, &ltriNrm, &emission, &ltriPdf)) {
      var ltriWi = normalize(ltriPos - ia.pos);
      let gsa = geomSolidAngle(ia.pos, ltriPos, ltriNrm);
      let brdf = evalMaterialCombined(mtl, ia.wo, ia.faceDir * ia.nrm, ltriWi);
      if(any(brdf > vec3f(0)) && !intersectTlasAnyHit(posOfs(ia.pos, ia.faceDir * ia.nrm), posOfs(ltriPos, ltriNrm))) {
        col += throughput * brdf * gsa * emission * max(0.0, dot(ia.faceDir * ia.nrm, ltriWi)) / ltriPdf;
      }
    }

    // Sample material
    var wi: vec3f;
    var specProb: f32;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, r1.xyz, &wi, &wasSpecular, &specProb, &pdf)) {
      break;
    }
    throughput *= evalMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, wi, wasSpecular, specProb) * dot(ia.nrm, wi); // * ia.faceDir

    // Russian roulette
    // Terminate with prob inverse to throughput
    let p = min(0.95, maxComp3(throughput));
    if(r1.w > p) {
      break;
    }
    // Account for bias introduced by path termination (pdf = p)
    // Boost surviving paths by their probability to be terminated
    throughput *= 1.0 / p;
    
    // Next ray
    ori = posOfs(ia.pos, ia.faceDir * ia.nrm);
    dir = wi;
  }

  return col;
}

fn renderNaive(oriPrim: vec3f, dirPrim: vec3f) -> vec3f
{
  var col = vec3f(0);
  var throughput = vec3f(1);
  var ori = oriPrim;
  var dir = dirPrim;

  for(var bounces=0u; bounces<globals.maxBounces; bounces++) {

    var hit = intersectTlas(ori, dir, INF);
    if(hit.t == INF) {
      col += throughput * globals.bgColor;
      break;
    }

    var ia: IA;
    var mtl: Mtl;
    finalizeHit(ori, dir, hit, &ia, &mtl);

    // Hit a light
    if(isEmissive(mtl)) {
      col += throughput * mtl.col * step(0.0, ia.faceDir);
      break;
    }

    let r0 = rand4();

    // Sample material
    var wi: vec3f;
    var isSpecular: bool;
    var specProb: f32;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, r0.xyz, &wi, &isSpecular, &specProb, &pdf)) {
      break;
    }
    throughput *= evalMaterial(mtl, ia.wo, ia.faceDir * ia.nrm, wi, isSpecular, specProb) * dot(ia.nrm, wi); // * ia.faceDir

    // Russian roulette
    // Terminate with prob inverse to throughput
    let p = min(0.95, maxComp3(throughput));
    if(r0.w > p) {
      break;
    }
    // Account for bias introduced by path termination (pdf = p)
    // Boost surviving paths by their probability to be terminated
    throughput *= 1.0 / p;

    // Next ray
    ori = posOfs(ia.pos, ia.faceDir * ia.nrm);
    dir = wi;
  }

  return col;
}

@compute @workgroup_size(8,8)
fn computeMain(@builtin(global_invocation_id) globalId: vec3u)
{
  if(any(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  var col = vec3f(0);
  for(var i=0u; i<globals.spp; i++) {
    rng = vec4u(globalId.xy, u32(globals.frame), i);
    let r0 = rand4();
    let eye = sampleEye(r0.xy);
    col += renderMIS(eye, normalize(samplePixel(vec2f(globalId.xy), r0.zw) - eye));
  }

  let index = globals.width * globalId.y + globalId.x;
  buffer[index] = vec4f(mix(buffer[index].xyz, col / f32(globals.spp), globals.weight), 1);
}

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  // Workaround for below code which does not pass naga
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
fn fragmentMain(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  // TODO Postprocessing
  return vec4f(pow(buffer[globals.width * u32(pos.y) + u32(pos.x)].xyz, vec3f(0.4545)), 1);
}
