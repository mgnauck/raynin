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
  let tmax = minComp4(vec4f(max(t0, t1), tfar));
  
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

  let invDet = 1.0 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  let u = dot(tvec, pvec) * invDet;
  if(u < 0.0 || u > 1.0) {
    return;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  let v = dot(dir, qvec) * invDet;
  if(v < 0.0 || u + v > 1.0) {
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
  var nodeStack: array<u32, MAX_NODE_CNT>;

  // Sorted DF traversal, visit near child first
  loop {
    let node = &nodes[blasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect contained triangle
      let nodeIdx = (*node).idx;
      let tri = tris[dataOfs + nodeIdx];
      intersectTri(ori, dir, tri.v0, tri.v1, tri.v2, (nodeIdx << 16) | (instId & SHORT_MASK), hit);
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
      continue;
    }

    // Interior node
    var childIndices = array<u32, 2>( nodeChildren & SHORT_MASK, nodeChildren >> 16 );

    let leftChildNode = &nodes[blasOfs + childIndices[0]];
    let rightChildNode = &nodes[blasOfs + childIndices[1]];

    // Intersect both child node aabbs
    var childDists = array<f32, 2>(
      intersectAabb(ori, invDir, (*hit).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
      intersectAabb(ori, invDir, (*hit).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1, 0, childDists[0] < childDists[1]);
    let far = 1 - near;

    if(childDists[near] == INF) {
      // Missed both children
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    } else {
      // Continue with nearer child node
      nodeIndex = childIndices[near];
      // Push farther child on stack if also within distance
      if(childDists[far] < INF) {
        nodeStack[nodeStackIndex] = childIndices[far];
        nodeStackIndex++;
      }
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

/*fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> Hit
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var node = nodes[tlasOfs]; // Start at root

  var nodeStackIndex = 0u;
  var nodeStack: array<Node, 32>;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal, visit near child first
  loop {
    var childNodes = array<Node, 2u>( // Naga won't allow 'let'
      nodes[tlasOfs + (node.children & SHORT_MASK)],
      nodes[tlasOfs + (node.children >> 16)] );

    // Intersect both child node aabbs
    var childDists = array<f32, 2>( // Naga won't allow 'let'
      intersectAabb(ori, invDir, hit.t, childNodes[0].aabbMin, childNodes[0].aabbMax),
      intersectAabb(ori, invDir, hit.t, childNodes[1].aabbMin, childNodes[1].aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1, 0, childDists[0] < childDists[1]);
    let far = 1 - near;

    // Intersect if leaf nodes
    if(childDists[near] < INF) {

      if(childNodes[near].children == 0) { 
        intersectInst(ori, dir, instances[childNodes[near].idx], &hit);
      }

      // hit.t might have been updated by above intersection tests
      if(childDists[far] < hit.t && childNodes[far].children == 0) {
        intersectInst(ori, dir, instances[childNodes[far].idx], &hit);
      }

      let traverseNear = childDists[near] != INF && childNodes[near].children != 0;
      let traverseFar = childDists[far] != INF && childNodes[far].children != 0;

      if(traverseNear) {
        // Continue with near child
        node = childNodes[near];
        // Push farther child on stack if also within distance
        if(traverseFar) {
          nodeStack[nodeStackIndex] = childNodes[far];
          nodeStackIndex++;
        }
        continue;
      } else if(traverseFar) {
        node = childNodes[far];
        continue;
      }
    }

    // Missed both children
    // Check the stack and continue traversal if something left
    if(nodeStackIndex == 0) {
      return hit;
    }
    nodeStackIndex--;
    node = nodeStack[nodeStackIndex];
  }

  return hit; // Required for Naga, Tint will warn on this
}*/

/*fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> Hit
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var node = nodes[tlasOfs]; // Start at root

  var nodeStackIndex = 0u;
  var nodeStack: array<Node, 32>;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal, visit near child first, less branching/divergence
  loop {
    var childNodes = array<Node, 2u>( // Naga won't allow 'let'
      nodes[tlasOfs + (node.children & SHORT_MASK)],
      nodes[tlasOfs + (node.children >> 16)] );

    // Intersect both child node aabbs
    var childDists = array<f32, 2>( // Naga won't allow 'let'
      intersectAabb(ori, invDir, hit.t, childNodes[0].aabbMin, childNodes[0].aabbMax),
      intersectAabb(ori, invDir, hit.t, childNodes[1].aabbMin, childNodes[1].aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1, 0, childDists[0] < childDists[1]);
    let far = 1 - near;

    // Intersect if leaf nodes
    if(childDists[near] < INF && childNodes[near].children == 0) { 
      intersectInst(ori, dir, instances[childNodes[near].idx], &hit);
    }

    // hit.t might have been updated by above intersection tests
    if(childDists[far] < hit.t && childNodes[far].children == 0) {
      intersectInst(ori, dir, instances[childNodes[far].idx], &hit);
    }

    let traverseNear = childDists[near] != INF && childNodes[near].children != 0;
    let traverseFar = childDists[far] != INF && childNodes[far].children != 0;

    if(!traverseNear && !traverseFar) {
      // Missed both children
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return hit;
      }
      nodeStackIndex--;
      node = nodeStack[nodeStackIndex];
    } else {
      if(traverseNear) {
        // Continue with near child
        node = childNodes[near];
        // Push farther child on stack if also within distance
        if(traverseFar) {
          nodeStack[nodeStackIndex] = childNodes[far];
          nodeStackIndex++;
        }
      } else {
        node = childNodes[far];
      }
    }
  }

  return hit; // Required for Naga, Tint will warn on this
}*/

fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> Hit
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;
  var nodeStack: array<u32, MAX_NODE_CNT>;

  var hit: Hit;
  hit.t = tfar;

  // Ordered DF traversal, visit near child first
  loop {
    let node = &nodes[tlasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect the single assigned instance 
      intersectInst(ori, dir, instances[(*node).idx], &hit);
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return hit;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
      continue;
    }

    // Interior node
    var childIndices = array<u32, 2>( nodeChildren & SHORT_MASK, nodeChildren >> 16 );

    let leftChildNode = &nodes[tlasOfs + childIndices[0]];
    let rightChildNode = &nodes[tlasOfs + childIndices[1]];

    // Intersect both child node aabbs
    var childDists = array<f32, 2>(
      intersectAabb(ori, invDir, hit.t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
      intersectAabb(ori, invDir, hit.t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1, 0, childDists[0] < childDists[1]);
    let far = 1 - near;

    if(childDists[near] == INF) {
      // Missed both children
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return hit;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    } else {
      // Continue with near child
      nodeIndex = childIndices[near];
      // Push farther child on stack if also within distance
      if(childDists[far] < INF) {
        nodeStack[nodeStackIndex] = childIndices[far];
        nodeStackIndex++;
      } 
    }
  }

  return hit; // Required for Naga, Tint will warn on this
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
