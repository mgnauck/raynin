struct Frame
{
  width:        u32,
  height:       u32,
  frame:        u32,
  bouncesSpp:   u32             // Bits 8-31 for gathered spp, bits 0-7 max bounces 
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

struct PathState
{
  seed:         vec4u,          // Last rng seed used
  throughput:   vec3f,
  pdf:          f32,
  ori:          vec3f,
  pad0:         f32,
  dir:          vec3f,
  pidx:         u32             // Pixel idx in bits 8-31, bounce num in bits 0-7
}

struct PathStateBuffer
{
  cnt:          vec4u,
  buf:          array<PathState>
}

struct Hit
{
  t:            f32,
  u:            f32,
  v:            f32,
  e:            u32             // (tri id << 16) | (inst id & 0xffff)
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
const EPS                 = 0.0001;
const INF                 = 3.402823466e+38;

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<uniform> instances: array<Inst, 1024>; // Uniform buffer max is 64k bytes
@group(0) @binding(2) var<storage, read> tris: array<Tri>;
@group(0) @binding(3) var<storage, read> nodes: array<Node>;
@group(0) @binding(4) var<storage, read> pathState: PathStateBuffer;
@group(0) @binding(5) var<storage, read_write> hits: array<Hit>;

// Traversal stacks
const MAX_NODE_CNT = 32u;
var<private> blasNodeStack: array<u32, MAX_NODE_CNT>;
var<private> tlasNodeStack: array<u32, MAX_NODE_CNT>;

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

fn intersectPlane(ori: vec3f, dir: vec3f, instId: u32, h: ptr<function, Hit>) -> bool
{
  let d = dir.y;
  if(abs(d) > EPS) {
    let t = -ori.y / d;
    if(t < (*h).t && t > EPS) {
      (*h).t = t;
      (*h).e = (ST_PLANE << 16) | (instId & SHORT_MASK);
      return true;
    }
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
      (*h).e = (ST_BOX << 16) | (instId & SHORT_MASK);
      return true;
    }
    if(tmax < (*h).t && tmax > EPS) {
      (*h).t = tmax;
      (*h).e = (ST_BOX << 16) | (instId & SHORT_MASK);
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
  (*h).e = (ST_SPHERE << 16) | (instId & SHORT_MASK);
  return true;
}

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f, instTriId: u32, h: ptr<function, Hit>)
{
  // Vectors of two edges sharing vertex 0
  let edge1 = v1 - v0;
  let edge2 = v2 - v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < EPS) {
    // Ray in plane of triangle
    return;
  }

  let invDet = 1 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  let u = dot(tvec, pvec) * invDet;
  if(u < 0 || u > 1) {
    return;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  let v = dot(dir, qvec) * invDet;
  if(v < 0 || u + v > 1) {
    return;
  }

  // Ray intersects, calc distance
  let dist = dot(edge2, qvec) * invDet;
  if(dist > EPS && dist < (*h).t) {
    (*h).t = dist;
    (*h).u = u;
    (*h).v = v;
    (*h).e = instTriId;
  }
}

fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, Hit>)
{
  let blasOfs = dataOfs << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  // Sorted DF traversal (near child first + skip far child if not within current dist)
  loop {
    let node = &nodes[blasOfs + nodeIndex];
    let nodeChildren = (*node).children;
   
    if(nodeChildren == 0) {
      // Leaf node, intersect contained triangle
      let nodeIdx = (*node).idx;
      let tri = tris[dataOfs + nodeIdx];
      intersectTri(ori, dir, tri.v0, tri.v1, tri.v2, (nodeIdx << 16) | (instId & SHORT_MASK), hit);
    } else {
      // Interior node
      var leftChildIndex = nodeChildren & SHORT_MASK;
      var rightChildIndex = nodeChildren >> 16;

      let leftChildNode = &nodes[blasOfs + leftChildIndex];
      let rightChildNode = &nodes[blasOfs + rightChildIndex];

      // Intersect both child node aabbs
      var leftDist = intersectAabb(ori, invDir, (*hit).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      var rightDist = intersectAabb(ori, invDir, (*hit).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
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
          blasNodeStack[nodeStackIndex] = rightChildIndex;
          nodeStackIndex++;
        }
        continue;
      }
      // Missed both child nodes
    }
    // Check the stack and continue traversal if something left
    if(nodeStackIndex > 0) {
      nodeStackIndex--;
      nodeIndex = blasNodeStack[nodeStackIndex];
    } else {
      return;
    }
  }
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
      intersectUnitBox(oriObj, 1.0 / dirObj, inst.id, hit);
    }
    case (SHAPE_TYPE_BIT | ST_SPHERE): {
      intersectUnitSphere(oriObj, dirObj, inst.id, hit);
    }
    default: {
      // Mesh type
      intersectBlas(oriObj, dirObj, 1.0 / dirObj, inst.id, inst.data & MESH_SHAPE_MASK, hit);
    }
  }
}

fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> Hit
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal (near child first + skip far child if not within current dist)
  loop {
    let node = &nodes[tlasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    if(nodeChildren == 0) {
      // Leaf node, intersect the single assigned instance 
      intersectInst(ori, dir, instances[(*node).idx], &hit);
    } else {
      // Interior node
      var leftChildIndex = nodeChildren & SHORT_MASK;
      var rightChildIndex = nodeChildren >> 16;

      let leftChildNode = &nodes[tlasOfs + leftChildIndex];
      let rightChildNode = &nodes[tlasOfs + rightChildIndex];

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

@compute @workgroup_size(16, 16)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = frame.width * globalId.y + globalId.x;
  if(gidx >= pathState.cnt.x) {
    return;
  }

  let pathState = pathState.buf[gidx];
  hits[gidx] = intersectTlas(pathState.ori, pathState.dir, INF);
}
