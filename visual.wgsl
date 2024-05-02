// Dimension of quasi random sequence. Expected to be multiple of 4.
const SEQ_DIM = 16u;
const MAX_BOUNCES = 5u;
const SEQ_LEN = MAX_BOUNCES * SEQ_DIM;

struct Global
{
  width:        u32,
  height:       u32,
  spp:          u32,
  maxBounces:   u32,
  rngSeed1:     f32,
  rngSeed2:     f32,
  gatheredSpp:  f32,
  weight:       f32,
  bgColor:      vec3f,
  frame:        f32,
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
  randAlpha:    array<vec4f, 20u> // = SEQ_LEN / 4u
}

struct Ray
{
  ori:  vec3f,
  dir:  vec3f
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
  col: vec3f,             // Albedo/diffuse. If component > 1.0, obj emits light.
  refl: f32,              // Probability of reflection
  refr: f32,              // Probability of refraction
  fuzz: f32,              // Fuzziness = 1 - glossiness
  ior: f32,               // Refraction index of obj we are entering
  pad0: f32,
  att: vec3f,             // Attenuation of transmission per col comp (beer)
  pad1: f32
}

struct IA
{
  dist:     f32,
  pos:      vec3f,
  nrm:      vec3f,
  outDir:   vec3f,
  inDir:    vec3f,
  mtl:      Mtl,
  flags:    u32,
  faceDir:  f32
}

struct Tri
{
  v0:   vec3f,
  mtl:  u32,              // (mtl id & 0xffff)
  v1:   vec3f,
  pad1: f32,
  v2:   vec3f,
  pad2: f32,
  n0:   vec3f,
  pad3: f32,
  n1:   vec3f,
  pad4: f32,
  n2:   vec3f,
  pad5: f32,
//#ifdef TEXTURE_SUPPORT
/*uv0:  vec2f,
  uv1:  vec2f,
  uv2:  vec2f,
  pad6: f32,
  pad7: f32*/
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

// Interaction flags
const IA_SPECULAR         = 1u;

// Math constants
const EPSILON             = 0.0001;
const PI                  = 3.141592;
const INV_PI              = 1.0 / PI;
const MAX_DISTANCE        = 3.402823466e+38;
const MAX_NODE_CNT        = 32;
const MAX_INTENSITY       = 1.7;  // Value for intensity clamping, will need trial and error
const MAX_LTRIS           = 64;

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<storage, read> tris: array<Tri>;
@group(0) @binding(2) var<storage, read> ltris: array<LTri>;
@group(0) @binding(3) var<storage, read> indices: array<u32>;
@group(0) @binding(4) var<storage, read> bvhNodes: array<BvhNode>;
@group(0) @binding(5) var<storage, read> tlasNodes: array<TlasNode>;
@group(0) @binding(6) var<storage, read> instances: array<Inst>;
@group(0) @binding(7) var<storage, read> materials: array<Mtl>;
@group(0) @binding(8) var<storage, read_write> buffer: array<vec4f>;

var<private> bvhNodeStack: array<u32, MAX_NODE_CNT>;
var<private> tlasNodeStack: array<u32, MAX_NODE_CNT>;

// PRNG state
var<private> rngState: u32;

// Quasi random sequence and state
var<private> randSeq: array<vec4f, 20u>; // = SEQ_LEN / 4u
var<private> randIdx: u32 = 0;

// Global so we can assign some error color from everywhere
var<private> throughput: vec3f;

fn minComp(v: vec3f) -> f32
{
  return min(v.x, min(v.y, v.z));
}

fn maxComp(v: vec3f) -> f32
{
  return max(v.x, max(v.y, v.z));
}

fn toMat3x3(m: mat3x4f) -> mat3x3f
{
  return mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);
}

fn toMat4x4(m: mat3x4f) -> mat4x4f
{
  return mat4x4f(m[0], m[1], m[2], vec4f(0, 0, 0, 1));
}

// PCG from https://jcgt.org/published/0009/03/02/
fn rand() -> f32
{
  rngState = rngState * 747796405u + 2891336453u;
  let word = ((rngState >> ((rngState >> 28u) + 4u)) ^ rngState) * 277803737u;
  //return f32((word >> 22u) ^ word) / f32(0xffffffffu);
  return ldexp(f32((word >> 22u) ^ word), -32);
}

fn rand4() -> vec4f
{
  return vec4f(rand(), rand(), rand(), rand());
}

// Martin Roberts, https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/ 
fn initQRandSeq(n: f32, seed: f32)
{
  randIdx = 0u;

  // Expects dim to be multiple of 4, because of alphas being a vec4f
  for(var i=0u; i<SEQ_LEN / 4u; i++) {
    //randSeq[i] = fract(vec4f(seed) + (n + 1.0) * globals.randAlpha[i]);
    randSeq[i] = fract(vec4f(seed) + vec4f(u32(n + 1.0) * vec4u(globals.randAlpha[i] * 33554431)) / exp2(24.0));
  }
}

fn qrand4() -> vec4f
{
  let r = randSeq[randIdx];
  randIdx++;
  return r;
}

// https://mathworld.wolfram.com/SpherePointPicking.html
fn rand3UnitSphere(r: vec2f) -> vec3f
{
  let u = 2 * r.x - 1;
  let theta = 2 * PI * r.y;
  let radius = sqrt(1 - u * u);
  return vec3f(radius * cos(theta), radius * sin(theta), u);
}

fn rand3Hemi(nrm: vec3f, r: vec2f) -> vec3f
{
  // Uniform + rejection sampling
  let v = rand3UnitSphere(r);
  return select(-v, v, dot(v, nrm) > 0);
}

// http://psgraphics.blogspot.com/2019/02/picking-points-on-hemisphere-with.html
fn rand3HemiCosWeighted(nrm: vec3f, r: vec2f) -> vec3f
{
  // Expects nrm to be unit vector
  let dir = nrm + rand3UnitSphere(r);
  return select(normalize(dir), nrm, all(abs(dir) < vec3f(EPSILON)));
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn rand2Disk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(radius * cos(theta), radius * sin(theta));
}

// Parallelogram method
// https://mathworld.wolfram.com/TrianglePointPicking.html
fn randBarycentric(r: vec2f) -> vec3f
{
  if(r.x + r.y > 1.0) {
    return vec3f(1.0 - r.x, 1.0 - r.y, -1.0 + r.x + r.y);
  }

  return vec3f(r, 1 - r.x - r.y);
}

// Fibonacci
/*fn randBarycentric(r: vec2f) -> vec3f
{
  let rxSqrt = sqrt(r.x);
  let b = vec2f(1 - rxSqrt, r.y * rxSqrt);
  return vec3f(b, 1 - b.x - b.y);
}*/

/*fn clampIntensity(contribution: vec3f) -> vec3f
{
  return min(contribution, vec3f(1.0));
}*/

fn clampIntensity(contribution: vec3f) -> vec3f
{
  let val = maxComp(contribution);
  return select(contribution, contribution * (MAX_INTENSITY / val), val > MAX_INTENSITY);
}

fn isNan(v: f32) -> bool
{
  //return v != v;
  return select(true, false, v < 0.0 || 0.0 < v || v == 0.0);
}

fn fixNan3(v: vec3f) -> vec3f
{
  return select(v, vec3f(0), isNan(v.x + v.y + v.z));
}

fn transformRay(ray: Ray, m: mat4x4f) -> Ray
{
  return Ray((vec4f(ray.ori, 1.0) * m).xyz, ray.dir * mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz));
}

// GPU efficient slabs test [Laine et al. 2013; Afra et al. 2016]
// https://www.jcgt.org/published/0007/03/04/paper-lowres.pdf
fn intersectAabb(ori: vec3f, invDir: vec3f, tfar: f32, minExt: vec3f, maxExt: vec3f) -> f32
{
  let t0 = (minExt - ori) * invDir;
  let t1 = (maxExt - ori) * invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t1, t0));
  
  return select(MAX_DISTANCE, tmin, tmin <= tmax && tmin < tfar && tmax > EPSILON);
}

fn intersectAabbAnyHit(ori: vec3f, invDir: vec3f, tfar: f32, minExt: vec3f, maxExt: vec3f) -> bool
{
  let t0 = (minExt - ori) * invDir;
  let t1 = (maxExt - ori) * invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t1, t0));
  
  return tmin <= tmax && tmin < tfar && tmax > EPSILON;
}

fn intersectPlane(ray: Ray, instId: u32, h: ptr<function, Hit>) -> bool
{
  let d = ray.dir.y;
  if(abs(d) > EPSILON) {
    let t = -ray.ori.y / d;
    if(t < (*h).t && t > EPSILON) {
      (*h).t = t;
      (*h).e = (ST_PLANE << 16) | (instId & INST_ID_MASK);
      return true;
    }
  }
  return false;
}

fn intersectPlaneAnyHit(ray: Ray, tfar: f32) -> bool
{
  let d = ray.dir.y;
  if(abs(d) > EPSILON) {
    let t = -ray.ori.y / d;
    return t < tfar && t > EPSILON;
  }
  return false;
}

fn intersectUnitBox(ori: vec3f, invDir: vec3f, instId: u32, h: ptr<function, Hit>) -> bool
{
  let t0 = (vec3f(-1.0) - ori) * invDir;
  let t1 = (vec3f( 1.0) - ori) * invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t0, t1));
 
  if(tmin <= tmax) {
    if(tmin < (*h).t && tmin > EPSILON) {
      (*h).t = tmin;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
      return true;
    }
    if(tmax < (*h).t && tmax > EPSILON) {
      (*h).t = tmax;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
      return true;
    }
  }
  return false;
}

fn intersectUnitSphere(ray: Ray, instId: u32, h: ptr<function, Hit>) -> bool
{
  let a = dot(ray.dir, ray.dir);
  let b = dot(ray.ori, ray.dir);
  let c = dot(ray.ori, ray.ori) - 1.0;

  var d = b * b - a * c;
  if(d < 0.0) {
    return false;
  }

  d = sqrt(d);
  var t = (-b - d) / a;
  if(t <= EPSILON || (*h).t <= t) {
    t = (-b + d) / a;
    if(t <= EPSILON || (*h).t <= t) {
      return false;
    }
  }

  (*h).t = t;
  (*h).e = (ST_SPHERE << 16) | (instId & INST_ID_MASK);
  return true;
}

fn intersectUnitSphereAnyHit(ray: Ray, tfar: f32) -> bool
{
  let a = dot(ray.dir, ray.dir);
  let b = dot(ray.ori, ray.dir);
  let c = dot(ray.ori, ray.ori) - 1.0;

  var d = b * b - a * c;
  if(d < 0.0) {
    return false;
  }

  d = sqrt(d);
  var t = (-b - d) / a;
  if(t <= EPSILON || tfar <= t) {
    t = (-b + d) / a;
    return t > EPSILON && t < tfar;
  }

  return true;
}

// Moeller/Trumbore ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f, u: ptr<function, f32>, v: ptr<function, f32>) -> f32
{
  // Vectors of two edges sharing vertex 0
  let edge1 = v1 - v0;
  let edge2 = v2 - v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < EPSILON) {
    // Ray in plane of triangle
    return MAX_DISTANCE;
  }

  let invDet = 1 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  *u = dot(tvec, pvec) * invDet;
  if(*u < 0 || *u > 1) {
    return MAX_DISTANCE;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  *v = dot(dir, qvec) * invDet;
  if(*v < 0 || *u + *v > 1) {
    return MAX_DISTANCE;
  }

  // Calc distance
  return dot(edge2, qvec) * invDet;
}

fn intersectTriClosest(ray: Ray, tri: Tri, instTriId: u32, h: ptr<function, Hit>) -> bool
{
  var u: f32;
  var v: f32;
  let dist = intersectTri(ray.ori, ray.dir, tri.v0, tri.v1, tri.v2, &u, &v);
  if(dist > EPSILON && dist < (*h).t) {
    (*h).t = dist;
    (*h).u = u;
    (*h).v = v;
    (*h).e = instTriId;
    return true;
  }

  return false;
}

fn intersectTriAnyHit(ray: Ray, tfar: f32, tri: Tri) -> bool
{
  var u: f32;
  var v: f32;
  let dist = intersectTri(ray.ori, ray.dir, tri.v0, tri.v1, tri.v2, &u, &v);
  return dist > EPSILON && dist < tfar;
}

fn intersectBvh(ray: Ray, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, Hit>)
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
        intersectTriClosest(ray, tris[dataOfs + triId], (triId << 16) | (instId & INST_ID_MASK), hit);
      }
    } else {
      // Interior node
      var leftChildIndex = nodeStartIndex;
      var rightChildIndex = nodeStartIndex + 1;

      let leftChildNode = &bvhNodes[bvhOfs + leftChildIndex];
      let rightChildNode = &bvhNodes[bvhOfs + rightChildIndex];

      // Intersect both child node aabbs
      var leftDist = intersectAabb(ray.ori, invDir, (*hit).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      var rightDist = intersectAabb(ray.ori, invDir, (*hit).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
      // Swap for nearer child
      if(leftDist > rightDist) {
        let td = rightDist;
        rightDist = leftDist;
        leftDist = td;

        leftChildIndex += 1u;
        rightChildIndex -= 1u;
      }

      if(leftDist < MAX_DISTANCE) {
        // Continue with nearer child node
        nodeIndex = leftChildIndex;
        if(rightDist < MAX_DISTANCE) {
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

fn intersectBvhAnyHit(ray: Ray, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  let bvhOfs = dataOfs << 1;

  // Early exit, unordered DF traversal
  loop {
    let node = &bvhNodes[bvhOfs + nodeIndex];
    if(intersectAabbAnyHit(ray.ori, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeStartIndex = (*node).startIndex;
      let nodeObjCount = (*node).objCount; 
      if(nodeObjCount != 0) {
        // Leaf node, intersect all contained triangles
        for(var i=0u; i<nodeObjCount; i++) {
          let triId = indices[dataOfs + nodeStartIndex + i];
          if(intersectTriAnyHit(ray, tfar, tris[dataOfs + triId])) {
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

  return false;
}

fn intersectInst(ray: Ray, inst: Inst, hit: ptr<function, Hit>)
{
  // Transform ray into object space of the instance
  let rayObjSpace = transformRay(ray, toMat4x4(inst.invTransform));
  let invDir = 1 / rayObjSpace.dir;

  switch(inst.data & INST_DATA_MASK) {
    // Shape type
    case (SHAPE_TYPE_BIT | ST_PLANE): {
      intersectPlane(rayObjSpace, inst.id, hit);
    }
    case (SHAPE_TYPE_BIT | ST_BOX): {
      intersectUnitBox(rayObjSpace.ori, invDir, inst.id, hit);
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      intersectUnitSphere(rayObjSpace, inst.id, hit);
    }
    default: {
      // Mesh type
      intersectBvh(rayObjSpace, invDir, inst.id, inst.data & MESH_SHAPE_MASK, hit);
    }
  }
}

fn intersectInstAnyHit(ray: Ray, tfar: f32, inst: Inst) -> bool
{
  // Transform ray into object space of the instance
  let rayObjSpace = transformRay(ray, toMat4x4(inst.invTransform));
  let invDir = 1 / rayObjSpace.dir;

  switch(inst.data & INST_DATA_MASK) {
    // Shape type
    case (SHAPE_TYPE_BIT | ST_PLANE): {
      return intersectPlaneAnyHit(rayObjSpace, tfar);
    }
    case (SHAPE_TYPE_BIT | ST_BOX): {
      return intersectAabbAnyHit(rayObjSpace.ori, invDir, tfar, vec3f(-1), vec3f(1)); 
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      return intersectUnitSphereAnyHit(rayObjSpace, tfar);
    }
    default: {
      // Mesh type
      return intersectBvhAnyHit(rayObjSpace, invDir, tfar, inst.data & MESH_SHAPE_MASK);
    }
  }
}

fn intersectTlas(ray: Ray, tfar: f32) -> Hit
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  let invDir = 1 / ray.dir;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal (near child first + skip far child if not within current dist)
  loop {
    let node = &tlasNodes[nodeIndex];
    let nodeChildren = (*node).children;

    if(nodeChildren == 0) {
      // Leaf node, intersect the single assigned instance 
      intersectInst(ray, instances[(*node).inst], &hit);
    } else {
      // Interior node
      var leftChildIndex = nodeChildren & TLAS_NODE_MASK;
      var rightChildIndex = nodeChildren >> 16;

      let leftChildNode = &tlasNodes[leftChildIndex];
      let rightChildNode = &tlasNodes[rightChildIndex];

      // Intersect both child node aabbs
      var leftDist = intersectAabb(ray.ori, invDir, hit.t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      var rightDist = intersectAabb(ray.ori, invDir, hit.t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

      // Swap for nearer child
      if(leftDist > rightDist) {
        let td = rightDist;
        rightDist = leftDist;
        leftDist = td;

        let ti = rightChildIndex;
        rightChildIndex = leftChildIndex;
        leftChildIndex = ti;
      }

      if(leftDist < MAX_DISTANCE) {
        // Continue with nearer child node
        nodeIndex = leftChildIndex;
        if(rightDist < MAX_DISTANCE) {
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

  return hit;
}

fn intersectTlasAnyHit(ray: Ray, tfar: f32) -> bool
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  let invDir = 1 / ray.dir;

  // Early exit, unordered DF traversal
  loop {
    let node = &tlasNodes[nodeIndex];
    if(intersectAabbAnyHit(ray.ori, invDir, tfar, (*node).aabbMin, (*node).aabbMax)) {
      let nodeChildren = (*node).children;
      if(nodeChildren == 0) {
        // Leaf node, intersect the single assigned instance
        if(intersectInstAnyHit(ray, tfar, instances[(*node).inst])) {
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

  return false;
}

fn calcShapeNormal(inst: Inst, hitPos: vec3f) -> vec3f
{
  switch(inst.data & MESH_SHAPE_MASK) {
    case ST_BOX: {
      let pos = (vec4f(hitPos, 1.0) * toMat4x4(inst.invTransform)).xyz;
      var nrm = pos * step(vec3f(1.0) - abs(pos), vec3f(EPSILON));
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

fn evalMaterial(ia: ptr<function, IA>, pdf: ptr<function, f32>) -> vec3f
{
  // This is called for light sampling only, which is not executed for
  // specular. Thus eval only the diffuse term here.
  if(((*ia).flags & IA_SPECULAR) > 0) {
    // Error
    *pdf = 1.0;
    return vec3f(0, 0, 1000);
  }

  let lDotN = max(0.0, dot((*ia).inDir, (*ia).faceDir * (*ia).nrm));
  *pdf = lDotN * INV_PI;
  return (*ia).mtl.col * INV_PI * lDotN;
}

fn schlickReflectance(cosTheta: f32, iorRatio: f32) -> f32
{
  var r0 = (1.0 - iorRatio) / (1.0 + iorRatio);
  r0 *= r0;
  return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

// https://computergraphics.stackexchange.com/questions/2482/choosing-reflection-or-refraction-in-path-tracing
fn sampleMaterial(ia: ptr<function, IA>, r: vec4f, pdf: ptr<function, f32>) -> vec3f
{
  // Flip the normal if we are inside or seeing a backface of not closed mesh
  let nrm = (*ia).faceDir * (*ia).nrm;
  if(r.x < (*ia).mtl.refr) {
    // Specular refraction
    let iorRatio = select(1 /* air */ / (*ia).mtl.ior, (*ia).mtl.ior, (*ia).faceDir < 0);
    let cosTheta = min(dot((*ia).outDir, nrm), 1.0);
    (*ia).inDir = refract(-(*ia).outDir, nrm, iorRatio);
    (*ia).flags |= IA_SPECULAR;
    *pdf = 1.0;
    // Apply fuzziness to reflection and refraction paths
    if(r.y < schlickReflectance(cosTheta, iorRatio) || all((*ia).inDir == vec3f(0))) {
      let reflDir = reflect(-(*ia).outDir, nrm);
      (*ia).inDir = normalize(reflDir + (*ia).mtl.fuzz * rand3Hemi(reflDir, r.zw));
    } else {
      (*ia).inDir = normalize((*ia).inDir + (*ia).mtl.fuzz * rand3Hemi((*ia).inDir, r.zw));
    }
    let beer = exp(-(*ia).mtl.att * (*ia).dist) * step((*ia).faceDir, 0.0) + step(0.0, (*ia).faceDir);
    return (*ia).mtl.col * beer / abs(dot((*ia).inDir, nrm));
  } else {
    if(r.y < (*ia).mtl.refl) {
      // Specular reflection
      let reflDir = reflect(-(*ia).outDir, nrm);
      (*ia).inDir = normalize(reflDir + (*ia).mtl.fuzz * rand3Hemi(reflDir, r.zw));
 	    (*ia).flags |= IA_SPECULAR;
      *pdf = 1.0;
      return (*ia).mtl.col / abs(dot((*ia).inDir, nrm));
    } else {
      // Diffuse
      (*ia).inDir = rand3HemiCosWeighted(nrm, r.zw);
      *pdf = max(0.0, dot((*ia).inDir, nrm)) * INV_PI;
      return (*ia).mtl.col * INV_PI * abs(dot((*ia).inDir, nrm));
    }
  }
}

fn calcLTriContribution(ia: ptr<function, IA>, ltriIdx: u32, bc: vec3f) -> f32
{
  // Uniform
  //return 1.0 / f32(arrayLength(&ltris));

  let ltri = &ltris[ltriIdx];
  var lightDir = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z - (*ia).pos;

  let invDist = 1.0 / length(lightDir);
  lightDir *= invDist;

  // Inclination between light normal and light dir
  // (how much the light faces the light dir)
  let lnDotL = max(0.0, -dot((*ltri).nrm, lightDir));

  // Inclination between surface normal and light dir
  // (how much the surface aligns with the light dir)
  let nDotL = max(0.0, dot((*ia).faceDir * (*ia).nrm, lightDir));

  // Inclination angles scale our measure of light power / dist^2
  return lnDotL * nDotL * (*ltri).power * invDist * invDist;
}

// https://computergraphics.stackexchange.com/questions/4792/path-tracing-with-multiple-lights/
fn pickLTriRandomly(ia: ptr<function, IA>, r: f32, bc: vec3f, ltriIdx: ptr<function, u32>, pdf: ptr<function, f32>)
{
  // Calculate picking probability with respect to ltri contributions
  var contributions: array<f32, MAX_LTRIS>;
  var totalContrib = 0.0;
  let ltriCnt = arrayLength(&ltris);

  // Calc contribution of each ltri and total of all contributions
  for(var i=0u; i<ltriCnt; i++) {
    let curr = calcLTriContribution(ia, i, bc);
    contributions[i] = curr;
    totalContrib += curr;
  }

  // No ltri can be picked, i.e. each ltri faces away
  if(totalContrib < EPSILON) {
    *pdf = 0.0;
    return;
  }

  // Same as scaling contributions[i] by totalContrib
  let rScaled = r * totalContrib;

  // Randomly pick the ltri according to the CDF
  // CDF is pdf for value X or all smaller ones
  var cumulative = 0.0;
  for(var i=0u; i<ltriCnt; i++) {
    cumulative += contributions[i];
    if(rScaled <= cumulative) {
      *ltriIdx = i;
      break;
    }
  }

  // Scale contributions by total, so that our picking pdf integrates to 1
  *pdf = contributions[*ltriIdx] / totalContrib;
}

fn pdfLTri(ia: ptr<function, IA>, ltri: LTri, dist: f32) -> f32
{
  // Account for single ray in direction of light
  // pdf is 1 / solid angle subtended by the light tri (area projected on unit hemisphere)
  return (dist * dist) / (ltri.area * dot(-(*ia).inDir, ltri.nrm));
}

fn evalLTri(ia: ptr<function, IA>, ltri: LTri, pdf: ptr<function, f32>) -> vec3f
{
  if(dot((*ia).inDir, ltri.nrm) > 0.0) {
    // Light is facing away (lights are not double sided)
    *pdf = 0.0;
    return vec3f(0);
  }

  // Get us the distance to the light if we actually intersect it
  var u: f32;
  var v: f32;
  let dist = intersectTri((*ia).pos, (*ia).inDir, ltri.v0, ltri.v1, ltri.v2, &v, &u);
  if(dist == MAX_DISTANCE) {
    // Not hitting the light tri
    *pdf = 0.0;
    return vec3f(0);
  }

  // Check if light is obstructed
  if(intersectTlasAnyHit(Ray((*ia).pos, (*ia).inDir), dist - EPSILON)) {
    *pdf = 0.0;
    return vec3f(0);
  }

  // Visible, calc pdf for current direction
  *pdf = pdfLTri(ia, ltri, dist);
  return ltri.emission;
}

fn sampleLTri(ia: ptr<function, IA>, ltri: LTri, bc: vec3f, pdf: ptr<function, f32>) -> vec3f
{
  // Find random point on light surface and calc light dir
  var lightDir = ltri.v0 * bc.x + ltri.v1 * bc.y + ltri.v2 * bc.z - (*ia).pos;

  if(dot(lightDir, ltri.nrm) > 0) {
    // Light is facing away (lights are not double sided)
    *pdf = 0.0;
    return vec3f(0);
  }

  let dist = length(lightDir);
  lightDir *= 1.0 / dist;

  // Light dir is input dir to sampling material
  (*ia).inDir = lightDir;

  // Check if light is obstructed
  if(intersectTlasAnyHit(Ray((*ia).pos, (*ia).inDir), dist - EPSILON)) {
    *pdf = 0.0;
    return vec3f(0);
  }

  // Visible, calc pdf for current direction
  *pdf = pdfLTri(ia, ltri, dist);
  return ltri.emission;
}

// Direct light sampling: Choose light randomly based on picking prob. Sample light. Apply MIS.
// Veach/Guibas "Optimally Combining Sampling Techniques for Monte Carlo Rendering"
fn sampleDirectLight(ia: ptr<function, IA>, r0: vec3f, r1: vec4f) -> vec3f
{
  var contribution = vec3f(0);

  // Sample the light only if current bsdf is not specular
  if(((*ia).flags & IA_SPECULAR) > 0) {
    return contribution;
  }

  // Prepare random barycentric coordinates
  let bc = randBarycentric(r0.xy);

  // Pick ltri according to contribution and calc picking probability
  var pickPdf: f32;
  var ltriIdx: u32;
  pickLTriRandomly(ia, r0.z, bc, &ltriIdx, &pickPdf);
  if(pickPdf == 0) {
    return contribution;
  }
  
  let ltri = &ltris[ltriIdx];

  // Sample light tri (sets direction to random point on ltri)
  var ltriPdf: f32;
  let emission = sampleLTri(ia, *ltri, bc, &ltriPdf);
  if(ltriPdf < EPSILON) {
    return contribution;
  }
  ltriPdf *= pickPdf;

  // Scale by the probability of the mtl producing a diffuse bounce
  // because the bsdf can produce specular reflection/refraction
  var bsdfPdf: f32;
  var bsdf = evalMaterial(ia, &bsdfPdf) * (1 - (*ia).mtl.refl - (*ia).mtl.refr);
  if(bsdfPdf > 0) {
    // Power heuristic with beta = 2
    let weight = ltriPdf * ltriPdf / (bsdfPdf * bsdfPdf + ltriPdf * ltriPdf);
    contribution += emission * bsdf * weight / ltriPdf;
  }

  // Sample bsdf (sets direction according to material bsdf)
  bsdf = sampleMaterial(ia, r1, &bsdfPdf);
  if(bsdfPdf > 0) {
    let emission = evalLTri(ia, *ltri, &ltriPdf);
    if(ltriPdf > 0) {
      ltriPdf *= pickPdf;
      let weight = bsdfPdf * bsdfPdf / (bsdfPdf * bsdfPdf + ltriPdf * ltriPdf);
      contribution += emission * bsdf * weight / bsdfPdf;
    }
  }

  return contribution;
}

fn render(initialRay: Ray) -> vec3f
{
  var ia: IA;
  var ray = initialRay;
  var col = vec3f(0);
  //var throughput = vec3f(1);

  throughput = vec3f(1);

  for(var bounces=0u; bounces<globals.maxBounces; bounces++) {
    // Intersect with scene
    let hit = intersectTlas(ray, MAX_DISTANCE);

    // No hit
    if(hit.t == MAX_DISTANCE) {
      //col += clampIntensity(throughput * globals.bgColor);
      col += throughput * globals.bgColor;
      break;
    }

    // Finalize hit data
    let inst = instances[hit.e & INST_ID_MASK];

    ia.dist = hit.t;
    ia.pos = ray.ori + hit.t * ray.dir;
    ia.outDir = -ray.dir;
 
    if((inst.data & SHAPE_TYPE_BIT) > 0) {
      // Shape
      ia.mtl = materials[(inst.id >> 16) & MTL_ID_MASK];
      ia.nrm = calcShapeNormal(inst, ia.pos);
    } else {
      // Mesh
      let ofs = inst.data & MESH_SHAPE_MASK;
      let tri = tris[ofs + (hit.e >> 16)];
      // Either use the material id from the triangle or the material override from the instance
      ia.mtl = materials[select(tri.mtl, inst.id >> 16, (inst.data & MTL_OVERRIDE_BIT) > 0) & MTL_ID_MASK];
      ia.nrm = calcTriNormal(hit, inst, tri);
    }

    // Hit a light
    if(isEmissive(ia.mtl)) {
      // Only consider if a primary ray or last bounce was specular
      if((bounces == 0u || ((ia.flags & IA_SPECULAR) > 0u)) &&
        // Lights have no back side
        (dot(ia.outDir, ia.nrm) > EPSILON)) {
        //col += clampIntensity(throughput * ia.mtl.col);
        col += throughput * ia.mtl.col;
      }
      break;
    }

    // Indicate backside hitfloat
    ia.faceDir = select(1.0, -1.0, dot(ia.outDir, ia.nrm) < EPSILON);

    // Low budget path regularization. Breaks recursive things but prevents some numerical instabilities/fireflies.
    // Kaplanyan + Frenetic/CubicTeami&$een: Path Space Regularization
    ia.mtl.refl = select(ia.mtl.refl, 0.0, bounces > 0);

    // Flag reflection or refraction materials, to exclude from light sampling
    ia.flags = select(ia.flags & ~IA_SPECULAR, ia.flags | IA_SPECULAR,
                      (ia.mtl.refr > 0.5 || ia.mtl.refl > 0.01));

    let r0 = qrand4();
    let r1 = qrand4();
    let r2 = qrand4();

    // Sample direct light
    //col += clampIntensity(throughput * sampleDirectLight(&ia, r0.xyz, r1));
    col += throughput * sampleDirectLight(&ia, r0.xyz, r1);

    // Sample indirect light
    var bsdfPdf: f32;
    let bsdf = sampleMaterial(&ia, r2, &bsdfPdf);
    if(bsdfPdf > EPSILON) {
      throughput *= bsdf / bsdfPdf;
    } else {
      break;
    }

    // Russian roulette
    // Terminate with prob inverse to throughput, except for primary ray or specular interaction
    //let p = select(maxComp(throughput), 1.0, bounces == 0 || (ia.flags & IA_SPECULAR) > 0);
    let p = maxComp(throughput);
    if(r0.w > p) {
      break;
    }
    // Account for bias introduced by path termination (pdf = p)
    // Boost surviving paths by their probability to be terminated
    throughput *= 1.0 / p;

    // Next ray to trace
    ray = Ray(ia.pos + ia.inDir * EPSILON, ia.inDir);
  }

  return col;
}

fn createPrimaryRay(pixelPos: vec2f, r: vec4f) -> Ray
{
  var pixelSample = globals.pixelTopLeft + globals.pixelDeltaX * pixelPos.x + globals.pixelDeltaY * pixelPos.y;
  pixelSample += (r.x - 0.5) * globals.pixelDeltaX + (r.y - 0.5) * globals.pixelDeltaY;

  var eyeSample = globals.eye;
  if(globals.focAngle > 0) {
    let focRadius = globals.focDist * tan(0.5 * radians(globals.focAngle));
    let diskSample = rand2Disk(r.zw);
    eyeSample += focRadius * (diskSample.x * globals.right + diskSample.y * globals.up);
  }

  return Ray(eyeSample, normalize(pixelSample - eyeSample));
}

@compute @workgroup_size(8,8)
fn computeMain(@builtin(global_invocation_id) globalId: vec3u)
{
  if(any(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  let index = globals.width * globalId.y + globalId.x;

  // White noise
  rngState = index ^ u32(globals.rngSeed1 * 0xffffffff);

  // Pixel offset into quasirandom sequence
  // Martin Roberts, https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/ 
  var randOfs = fract(dot(vec2f(0.754877669, 0.569840296), vec2f(globalId.xy)));
  randOfs = mix(2.0 * randOfs, 2.0 - 2.0 * randOfs, step(0.5, randOfs));

  // Pixel offset via interleaved gradient noise
  //randOfs = fract(52.9829189 * fract(0.06711056 * f32(globalId.x) + 0.00583715 * f32(globalId.y))); 

  var col = vec3f(0);
  for(var i=0u; i<globals.spp; i++) {
    initQRandSeq(globals.gatheredSpp + f32(i), randOfs);
    col += render(createPrimaryRay(vec2f(globalId.xy), qrand4()));
  }

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
