struct Global
{
  width:        u32,
  height:       u32,
  spp:          u32,
  maxBounces:   u32,
  frame:        u32,
  gatheredSpp:  u32,
  pad0:         u32,
  pad1:         u32,
  bgColor:      vec3f,
  weight:       f32,
  eye:          vec3f,
  vertFov:      f32,
  right:        vec3f,
  focDist:      f32,
  up:           vec3f,
  focAngle:     f32,
  pixelDeltaX:  vec3f,
  pad2:         f32,
  pixelDeltaY:  vec3f,
  pad3:         f32,
  pixelTopLeft: vec3f,
  pad4:         f32,
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
  col:        vec3f,      // Base color (diff col of non-metallics, spec col of metallics)
  metallic:   f32,        // Appearance range from dielectric to conductor (0 - 1)
  roughness:  f32,        // Perfect reflection to completely diffuse (0 - 1)
  ior:        f32,        // Index of refraction
  refractive: f32,        // Flag if material refracts
  pad0:       f32
}

struct IA
{
  pos:      vec3f,
  dist:     f32,
  nrm:      vec3f,
  ltriId:   u32,
  wo:       vec3f
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
const INF                 = 3.402823466e+38;
const PI                  = 3.141592;
const TWO_PI              = 6.283185;
const INV_PI              = 1 / PI;

// posOfs
const ORIGIN              = vec3f(1 / 32.0);
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

// Traversal stacks for bvhs
const MAX_NODE_CNT = 32u;
var<private> bvhNodeStack: array<u32, MAX_NODE_CNT>;
var<private> tlasNodeStack: array<u32, MAX_NODE_CNT>;

// Ltri contributions
const MAX_LTRI_CNT = 256u;
var<private> ltriContributions: array<f32, MAX_LTRI_CNT>;

// State of prng
var<private> rng: vec4u;

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
// Hope the vectorization & conversion to wgsl is correct :)
fn posOfs(p: vec3f, n: vec3f) -> vec3f
{
  let ofInt = vec3i(INT_SCALE * n);
  let pInt = bitcast<vec3f>(bitcast<vec3i>(p) + select(ofInt, -ofInt, p < vec3f(0)));
  return select(pInt, p + FLOAT_SCALE * n, abs(p) < ORIGIN);
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
      // Bring hit pos to object space
      let pos = (vec4f(hitPos, 1.0) * toMat4x4(inst.invTransform)).xyz;
      // Calc normal
      var nrm = pos * step(vec3f(1.0) - abs(pos), vec3f(EPS));
      // Transform nrm to world space with transpose of inverse
      return normalize(nrm * transpose(toMat3x3(inst.invTransform))); // TODO Optimize with above
    }
    case ST_PLANE: {
      return normalize(vec3f(0.0, 1.0, 0.0) * transpose(toMat3x3(inst.invTransform)));
    }
    case ST_SPHERE: {
      let pos = (vec4f(hitPos, 1.0) * toMat4x4(inst.invTransform)).xyz; // TODO Optimize
      return normalize(vec3f(pos) * transpose(toMat3x3(inst.invTransform)));
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
  // Transform nrm to world space with transpose of inverse
  return normalize(nrm * transpose(toMat3x3(inst.invTransform)));
}

fn isEmissive(mtl: Mtl) -> bool
{
  return any(mtl.col > vec3f(1.0));
}

fn luminance(col: vec3f) -> f32
{
  return dot(col, vec3f(0.2126, 0.7152, 0.0722));
}

fn getRoughness(mtl: Mtl) -> f32
{
  return saturate(mtl.roughness * mtl.roughness - EPS) + EPS;
}

fn mtlToF0(mtl: Mtl) -> vec3f
{
  var f0: f32;
  f0 = (1.0 - mtl.ior) / (1.0 + mtl.ior);
  f0 *= f0;
 
  // For metallic materials we use the base color, otherwise the ior
  return mix(vec3f(f0), mtl.col, mtl.metallic);
}

// https://en.wikipedia.org/wiki/Schlick's_approximation
fn fresnelSchlick(cosTheta: f32, f0: vec3f) -> vec3f
{
  return f0 + (vec3f(1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

// Boksansky: Crash Cource in BRDF Implementation
// Walter et al: Microfacet Models for Refraction through Rough Surfaces
// https://agraphicsguynotes.com/posts/sample_microfacet_brdf/
fn distributionGGX(n: vec3f, h: vec3f, alpha: f32) -> f32
{
  let NoH = dot(n, h);
  let alphaSqr = alpha * alpha;
  let NoHSqr = NoH * NoH;
  let den = NoHSqr * alphaSqr + (1.0 - NoHSqr);
  return (step(EPS, NoH) * alphaSqr) / (PI * den * den);
}

fn geometryPartialGGX(v: vec3f, n: vec3f, h: vec3f, alpha: f32) -> f32
{
  var VoHSqr = dot(v, h);
  let chi = step(EPS, VoHSqr / dot(n, v));
  VoHSqr = VoHSqr * VoHSqr;
  let tan2 = (1.0 - VoHSqr) / VoHSqr;
  return (chi * 2.0) / (1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

fn sampleGGX(n: vec3f, r0: vec2f, alpha: f32) -> vec3f
{
  // Sample the microfacet normal
  let cosTheta = sqrt((1.0 - r0.x) / ((alpha * alpha - 1.0) * r0.x + 1.0));
  let sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  let phi = TWO_PI * r0.y;
  return normalize(createONB(n) * vec3f(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta));
}

fn sampleGGXPdf(n: vec3f, m: vec3f, alpha: f32) -> f32
{
  return distributionGGX(n, m, alpha) * abs(dot(n, m));
}

fn sampleReflection(mtl: Mtl, wo: vec3f, n: vec3f, m: vec3f, wi: ptr<function, vec3f>) -> f32
{
  if(dot(wo, m) <= 0.0) {
    return 0.0;
  }

  // Reflect at microfacet normal
  *wi = reflect(-wo, m);
  return sampleReflectionPdf(mtl, wo, n, *wi);
}

fn sampleReflectionPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  let h = sign(dot(n, wo)) * normalize(wo + wi);
  let dwhDwi = 4.0 * abs(dot(wo, h));
  return sampleGGXPdf(n, h, getRoughness(mtl)) / dwhDwi;
}

fn evalReflection(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  // Modulate by sign so that half vector works for front and back side
  let h = sign(dot(n, wo)) * normalize(wo + wi);

  let roughness = getRoughness(mtl);
  
  let distr = distributionGGX(n, h, roughness);
  let geom = geometryPartialGGX(wo, n, h, roughness) * geometryPartialGGX(wi, n, h, roughness);

  return distr * geom / max(0.05, 4.0 * abs(dot(n, wo)) * abs(dot(n, wi)));
}

fn sampleRefraction(mtl: Mtl, wo: vec3f, n: vec3f, m: vec3f, wi: ptr<function, vec3f>) -> f32
{
  if(dot(wo, m) <= 0.0) {
    return 0.0;
  }

  // Refract at microfacet normal
  let etaRatio = select(1 / mtl.ior, mtl.ior, dot(wo, n) < 0);
  *wi = refract(-wo, m, etaRatio);
  if(all(*wi == vec3f(0))) {
    return 0.0; // TIR
  }
 
  return sampleRefractionPdf(mtl, wo, n, *wi);
}

fn sampleRefractionPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  let etaI = select(1.0, mtl.ior, dot(wo, n) < 0); // incoming
  let etaT = select(mtl.ior, 1.0, dot(wo, n) < 0); // transmitted
  var h = -normalize(etaI * wo + etaT * wi);

  let denom = etaI * dot(wo, h) + etaT * dot(wi, h);
  let dwhDwi = etaT * etaT * abs(dot(wi, h)) / (denom * denom); 

  return sampleGGXPdf(n, h, getRoughness(mtl)) * dwhDwi;
}

fn evalRefraction(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> vec3f
{
  let etaI = select(1.0, mtl.ior, dot(wo, n) < 0);
  let etaT = select(mtl.ior, 1.0, dot(wo, n) < 0);
  var h = -normalize(etaI * wo + etaT * wi);

  let WIoH = dot(wi, h);
  let WOoH = dot(wo, h);

  let roughness = getRoughness(mtl);
  
  let distr = distributionGGX(n, h, roughness);
  let geom = geometryPartialGGX(wo, n, h, roughness) * geometryPartialGGX(wi, n, h, roughness);
  
  let denom = etaI * WOoH + etaT * WIoH;
  let a = etaT / denom;

  return mtl.col * distr * geom * a * a * abs(WIoH * WOoH / (dot(n, wo) * dot(n, wi)));
}

fn sampleDiffuse(n: vec3f, r0: vec2f, wi: ptr<function, vec3f>) -> f32
{
  *wi = createONB(n) * sampleHemisphereCos(r0);
  return sampleDiffusePdf(n, *wi);
}

fn sampleDiffusePdf(n: vec3f, wi: vec3f) -> f32
{
  return max(0.0, dot(n, wi)) * INV_PI;
}

fn evalDiffuse(mtl: Mtl, n: vec3f, wi: vec3f) -> vec3f
{
  return (1.0 - mtl.metallic) * mtl.col * INV_PI;
}

fn sampleMaterial(mtl: Mtl, wo: vec3f, n: vec3f, r0: vec3f, wi: ptr<function, vec3f>, fres: ptr<function, vec3f>, isSpecular: ptr<function, bool>, pdf: ptr<function, f32>) -> bool
{
  let m = sign(dot(wo, n)) * sampleGGX(n, r0.xy, getRoughness(mtl));
  *fres = fresnelSchlick(dot(wo, m), mtlToF0(mtl));
  let p = luminance(*fres);

  if(r0.z < p) {
    *isSpecular = true;
    *pdf = sampleReflection(mtl, wo, n, m, wi) * p;
  } else {
    *isSpecular = false;
    if(mtl.refractive > 0.0) { // Do not write as select, likely error in naga
      *pdf = sampleRefraction(mtl, wo, n, m, wi) * (1.0 - p);
    } else {
      *pdf = sampleDiffuse(n, r0.xy, wi) * (1.0 - p);
    }
  }

  return *pdf > 0.0;
}

fn sampleMaterialCombinedPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: vec3f) -> f32
{
  let f = luminance(fres);
  let otherPdf = select(sampleDiffusePdf(n, wi), sampleRefractionPdf(mtl, wo, n, wi), mtl.refractive > 0.0);
  return otherPdf * (1.0 - f) + sampleReflectionPdf(mtl, wo, n, wi) * f;
}

fn evalMaterial(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: vec3f, isSpecular: bool) -> vec3f
{
  if(isSpecular) {
    return evalReflection(mtl, wo, n, wi) * fres;
  } else {
    return select(evalDiffuse(mtl, n, wi), evalRefraction(mtl, wo, n, wi), mtl.refractive > 0.0) * (vec3f(1) - fres);
  }
}

fn evalMaterialCombined(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: ptr<function, vec3f>) -> vec3f
{
  if(dot(n, wi) > 0.0) {
    let h = normalize(wo + wi);
    *fres = fresnelSchlick(dot(wo, h), mtlToF0(mtl));
    let otherBsdf = select(evalDiffuse(mtl, n, wi), evalRefraction(mtl, wo, n, wi), mtl.refractive > 0.0);
    return otherBsdf * (vec3f(1) - *fres) + evalReflection(mtl, wo, n, wi) * (*fres);
  }

  return vec3f(0);
}

fn calcLTriContribution(pos: vec3f, nrm: vec3f, ltriPos: vec3f, ltriNrm: vec3f, lightPower: f32) -> f32
{
  var lightDir = ltriPos - pos;
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

fn calcLTriPickProb(pos: vec3f, nrm: vec3f, ltriPos: vec3f, ltriId: u32) -> f32
{
  // Calculate picking probability with respect to ltri contributions
  var totalContrib = 0.0;
  let ltriCnt = arrayLength(&ltris);

  // Calc contribution of each ltri with the given light dir/pos and total of all contributions
  for(var i=0u; i<ltriCnt; i++) {
    let ltri = &ltris[i];
    let curr = calcLTriContribution(pos, nrm, ltriPos, (*ltri).nrm, (*ltri).power);
    ltriContributions[i] = curr;
    totalContrib += curr;
  }

  // Scale contribution by total, so that our picking pdf integrates to 1
  return select(0.0, ltriContributions[ltriId] / totalContrib, totalContrib > EPS);
}

// https://computergraphics.stackexchange.com/questions/4792/path-tracing-with-multiple-lights/
fn pickLTriRandomly(pos: vec3f, nrm: vec3f, r: f32, bc: vec3f, pdf: ptr<function, f32>) -> u32
{
  // Calculate picking probability with respect to ltri contributions
  var totalContrib = 0.0;
  let ltriCnt = arrayLength(&ltris);

  // Calc contribution of each ltri and total of all contributions
  for(var i=0u; i<ltriCnt; i++) {
    let ltri = &ltris[i];
    let ltriPos = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;
    let curr = calcLTriContribution(pos, nrm, ltriPos, (*ltri).nrm, (*ltri).power);
    ltriContributions[i] = curr;
    totalContrib += curr;
  }

  // No ltri can be picked, i.e. each ltri faces away
  if(totalContrib < EPS) {
    *pdf = 0.0;
    return 0u;
  }

   // Same as scaling ltri contributions by totalContrib
  let rScaled = r * totalContrib;

  // Randomly pick the ltri according to the CDF
  // CDF is pdf for value X or all smaller ones
  var cumulative = 0.0;
  var ltriId: u32;
  for(var i=0u; i<ltriCnt; i++) {
    cumulative += ltriContributions[i];
    if(rScaled <= cumulative) {
      ltriId = i;
      break;
    }
  }

  // Scale contribution by total, so that our picking pdf integrates to 1
  *pdf = ltriContributions[ltriId] / totalContrib;
  return ltriId;
}

fn sampleLTris(pos: vec3f, n: vec3f, r0: vec3f, ltriPos: ptr<function, vec3f>, ltriNrm: ptr<function, vec3f>, emission: ptr<function, vec3f>, pdf: ptr<function, f32>) -> bool
{
  let bc = sampleBarycentric(r0.xy);

  // Pick a ltri uniformly
  //let ltriCnt = arrayLength(&ltris);
  //let ltriId = u32(floor(r0.z * f32(ltriCnt)));

  // Pick ltri via contribution
  var pickProb: f32;
  let ltriId = pickLTriRandomly(pos, n, r0.z, bc, &pickProb);
  if(pickProb == 0.0) {
    return false;
  }

  let ltri = &ltris[ltriId];

  *ltriPos = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;
  *ltriNrm = (*ltri).nrm;

  //let ldir = pos - *ltriPos;
  //var visible = dot(ldir, *ltriNrm) > 0; // Front side of ltri only
  //visible &= dot(ldir, n) < 0; // Not facing (behind)

  //*pdf = 1.0 / ((*ltri).area * f32(ltriCnt)); // Uniform
  *pdf = pickProb / (*ltri).area;

  *emission = (*ltri).emission;

  //return visible;
  return true;
}

fn sampleLTriPdf(pos: vec3f, n: vec3f, ltriPos: vec3f, ltriId: u32) -> f32
{
  // Uniform
  //return 1.0 / (ltris[ltriId].area * f32(arrayLength(&ltris)));

  let pickProb = calcLTriPickProb(pos, n, ltriPos, ltriId);
  return pickProb / ltris[ltriId].area;
}

// Lessig: The Area Formulation of Light Transport
fn geomSolidAngle(pos: vec3f, surfPos: vec3f, surfNrm: vec3f) -> f32
{
  // Distance attenuation
  // Converts from solid angle to surface area
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
    (*ia).ltriId = select(MAX_LTRI_CNT + 1u, tri.ltriId, isEmissive(*mtl));
  }

  // Flip normal if backside, except if we hit a ltri or refractive mtl
  (*ia).nrm *= select(-1.0, 1.0, dot((*ia).wo, (*ia).nrm) > 0 || isEmissive(*mtl) || (*mtl).refractive > 0.0);
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
    return mtl.col * step(0.0, dot(ia.wo, ia.nrm));
  }

  var col = vec3f(0);
  var throughput = vec3f(1);
  var wasSpecular = false;

  for(var bounces=0u; bounces<globals.maxBounces; bounces++) {

    let r0 = rand4();
    let r1 = rand4();

    // Sample lights directly (NEE)
    var ltriPos: vec3f;
    var ltriNrm: vec3f;
    var emission: vec3f;
    var ltriPdf: f32;
    if(sampleLTris(ia.pos, ia.nrm, r0.xyz, &ltriPos, &ltriNrm, &emission, &ltriPdf)) {
      // Apply MIS
      // Veach/Guibas: Optimally Combining Sampling Techniques for Monte Carlo Rendering
      var ltriWi = normalize(ltriPos - ia.pos);
      let gsa = geomSolidAngle(ia.pos, ltriPos, ltriNrm);
      var fres: vec3f;
      let bsdf = evalMaterialCombined(mtl, ia.wo, ia.nrm, ltriWi, &fres);
      let bsdfPdf = sampleMaterialCombinedPdf(mtl, ia.wo, ia.nrm, ltriWi, fres);
      let weight = ltriPdf / (ltriPdf + bsdfPdf * gsa);
      if(any(bsdf * gsa * weight > vec3f(0)) && !intersectTlasAnyHit(ia.pos, posOfs(ltriPos, ltriNrm))) {
        col += throughput * bsdf * gsa * weight * emission * saturate(dot(ia.nrm, ltriWi)) / ltriPdf;
      }
    }

    // Sample material
    var wi: vec3f;
    var fres: vec3f;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.nrm, r1.xyz, &wi, &fres, &wasSpecular, &pdf)) {
      break;
    }
    // Scale indirect light contribution by material
    throughput *= evalMaterial(mtl, ia.wo, ia.nrm, wi, fres, wasSpecular) * abs(dot(ia.nrm, wi)) / pdf;

    // Trace indirect light direction
    let ori = ia.pos;
    let dir = wi;
    hit = intersectTlas(ori, dir, INF);
    if(hit.t == INF) {
      col += throughput * globals.bgColor;
      break;
    }

    // Save for light hit MIS calculation
    let lastPos = ia.pos;
    let lastNrm = ia.nrm;

    finalizeHit(ori, dir, hit, &ia, &mtl);

    // Hit light via material direction
    if(isEmissive(mtl)) {
      // Lights emit from front side only
      if(dot(ia.wo, ia.nrm) > 0) {
        // Apply MIS
        let gsa = geomSolidAngle(lastPos, ia.pos, ia.nrm); // = ltri pos and ltri nrm
        let ltriPdf = sampleLTriPdf(lastPos, lastNrm, ia.pos, ia.ltriId);
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
      if(dot(ia.wo, ia.nrm) > 0) {
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
    if(sampleLTris(ia.pos, ia.nrm, r0.xyz, &ltriPos, &ltriNrm, &emission, &ltriPdf)) {
      var ltriWi = normalize(ltriPos - ia.pos);
      let gsa = geomSolidAngle(ia.pos, ltriPos, ltriNrm);
      var fres: vec3f;
      let bsdf = evalMaterialCombined(mtl, ia.wo, ia.nrm, ltriWi, &fres);
      if(any(bsdf > vec3f(0)) && !intersectTlasAnyHit(ia.pos, posOfs(ltriPos, ltriNrm))) {
        col += throughput * bsdf * gsa * emission * saturate(dot(ia.nrm, ltriWi)) / ltriPdf;
      }
    }

    // Sample material
    var wi: vec3f;
    var fres: vec3f;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.nrm, r1.xyz, &wi, &fres, &wasSpecular, &pdf)) {
      break;
    }
    throughput *= evalMaterial(mtl, ia.wo, ia.nrm, wi, fres, wasSpecular) * abs(dot(ia.nrm, wi)) / pdf;

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
    ori = ia.pos;
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
      col += throughput * mtl.col * step(0.0, dot(ia.wo, ia.nrm));
      break;
    }

    let r0 = rand4();

    // Sample material
    var wi: vec3f;
    var fres: vec3f;
    var isSpecular: bool;
    var pdf: f32;
    if(!sampleMaterial(mtl, ia.wo, ia.nrm, r0.xyz, &wi, &fres, &isSpecular, &pdf)) {
      break;
    }
    throughput *= evalMaterial(mtl, ia.wo, ia.nrm, wi, fres, isSpecular) * abs(dot(ia.nrm, wi)) / pdf;

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
    ori = ia.pos;
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
    rng = vec4u(globalId.xy, globals.frame, i);
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
fn fragmentMain(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  // TODO Postprocessing
  return vec4f(pow(buffer[globals.width * u32(pos.y) + u32(pos.x)].xyz, vec3f(0.4545)), 1);
}
