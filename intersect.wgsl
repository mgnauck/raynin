struct Config
{
  width:            u32,            // Bits 8-31 for width, bits 0-7 max bounces
  height:           u32,            // Bit 8-31 for height, bits 0-7 samples per pixel
  frame:            u32,            // Current frame number
  samplesTaken:     u32,            // Bits 8-31 for samples taken (before current frame), bits 0-7 frame's sample num
  pathCnt:          u32,
  extRayCnt:        u32,
  shadowRayCnt:     u32,
  pad0:             u32,
  gridDimPath:      vec4u,
  gridDimSRay:      vec4u,
  bgColor:          vec4f           // w is unused
}

struct Inst
{
  invTransform:     mat3x4f,
  id:               u32,            // (mtl override id << 16) | (inst id & 0xffff)
  data:             u32,            // See comment on data in inst.h
  pad0:             u32,
  pad1:             u32
}

struct Node
{
  aabbMin:          vec3f,
  children:         u32,            // 2x 16 bits for left and right child
  aabbMax:          vec3f,
  idx:              u32             // Assigned on leaf nodes only
}

struct Tri
{
  v0:               vec3f,
  mtl:              u32,            // (mtl id & 0xffff)
  v1:               vec3f,
  ltriId:           u32,            // Set only if tri has light emitting material
  v2:               vec3f,
  pad0:             f32,
  n0:               vec3f,
  pad1:             f32,
  n1:               vec3f,
  pad2:             f32,
  n2:               vec3f,
  pad3:             f32
}

struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, bounce num in bits 0-7
}

// Scene data handling
const SHORT_MASK          = 0xffffu;
const MESH_SHAPE_MASK     = 0x3fffffffu; // Bits 30-0

// General constants
const EPS                 = 0.0001;
const INF                 = 3.402823466e+38;

//const STACK_EMPTY_MARKER  = 0xfffffffi;

const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> instances: array<Inst, 1024>; // Uniform buffer max is 64k bytes
@group(0) @binding(1) var<storage, read> tris: array<Tri>;
@group(0) @binding(2) var<storage, read> nodes: array<Node>;
@group(0) @binding(3) var<storage, read> config: Config;
@group(0) @binding(4) var<storage, read> pathStates: array<PathState>;
@group(0) @binding(5) var<storage, read_write> hits: array<vec4f>;

// Traversal stacks
const MAX_NODE_CNT      = 64u;
const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
var<private> nodeStack: array<u32, MAX_NODE_CNT>;

// Syncs the workgroup execution
//var<workgroup> foundLeafCnt: atomic<u32>;

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

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f) -> vec3f
{
  // Vectors of two edges sharing vertex 0
  let edge1 = v1 - v0;
  let edge2 = v2 - v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < EPS) {
    // Ray in plane of triangle
    return vec3f(INF, 0.0, 0.0);
  }

  let invDet = 1.0 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  let u = dot(tvec, pvec) * invDet;
  if(u < 0.0 || u > 1.0) {
    return vec3f(INF, 0.0, 0.0);
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  let v = dot(dir, qvec) * invDet;
  if(v < 0.0 || u + v > 1.0) {
    return vec3f(INF, 0.0, 0.0);
  }

  // Ray intersects, calc distance
  return vec3f(dot(edge2, qvec) * invDet, u, v);
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
/*fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, vec4f>)
{
  let blasOfs = dataOfs << 1;

  var nodeStackIndex = HALF_MAX_NODE_CNT + 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (triangle) yet

  // Ordered DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {
      // Get current node's child indices
      let nodeChildren = nodes[blasOfs + u32(nodeIndex)].children;

      // Get child nodes
      let leftChildNode = &nodes[blasOfs + (nodeChildren & SHORT_MASK)];
      let rightChildNode = &nodes[blasOfs + (nodeChildren >> 16)];

      // Intersect both child node aabbs
      var childDists = array<f32, 2>(
        intersectAabb(ori, invDir, (*hit).x, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
        intersectAabb(ori, invDir, (*hit).x, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

      // Find indices of nearer and farther child
      let near = select(1u, 0u, childDists[0] < childDists[1]);
      let far = 1u - near;

      if(childDists[near] == INF) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children
        let childNodeIdx = array<u32, 2>( (*leftChildNode).idx, (*rightChildNode).idx );
        let childChildren = array<u32, 2>( (*leftChildNode).children, (*rightChildNode).children );

        // Set near child as next node
        // In case of leaf child, set its triangle index negated and incremented by one (to account for idx of 0)
        nodeIndex = select(-i32(childNodeIdx[near] + 1), i32((nodeChildren >> (near << 4u)) & SHORT_MASK), childChildren[near] > 0);

        if(childDists[far] < INF) {
          // Push far child on stack. Do the same as above in case this child is a leaf.
          nodeStack[nodeStackIndex] =
            select(-i32(childNodeIdx[far] + 1), i32((nodeChildren >> (far << 4u)) & SHORT_MASK), childChildren[far] > 0);
          nodeStackIndex++;
        }
      }

      // If our next node is a leaf node (as per above leaf nodes have a negative index) and we found no other leaf yet
      if(nodeIndex < 0 && leafIndex >= 0) {
        // Store the found leaf, so we can continue traversal
        leafIndex = nodeIndex;
        // Pop next node
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
        // Another one in the workgroup has found a leaf
        atomicAdd(&foundLeafCnt, 1u);
      }

      // Synchronize: Everyone in the workgroup has found a leaf
      if(atomicLoad(&foundLeafCnt) == WG_SIZE.x * WG_SIZE.y) {
        break;
      }
    }

    // Reset
    atomicStore(&foundLeafCnt, 0u);

    // Triangle "traversal"
    while(leafIndex < 0) {
      // Transform leaf index back into triangle index we had stored at the node
      let triIdx = u32(abs(leafIndex + 1));
      // Fetch tri data and intersect
      let tri = tris[dataOfs + triIdx];
      let tempHit = intersectTri(ori, dir, tri.v0, tri.v1, tri.v2);
      if(tempHit.x > EPS && tempHit.x < (*hit).x) {
        // Store closest hit only
        *hit = vec4f(tempHit, bitcast<f32>((triIdx << 16) | (instId & SHORT_MASK)));
      }
      // Set next potential leaf for processing
      leafIndex = nodeIndex;
      if(leafIndex < 0) {
        // Have another leaf/triangle available
        // Pop next node (which could very well be a leaf too)
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    }
  }
}*/

fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, vec4f>)
{
  let blasOfs = dataOfs << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = HALF_MAX_NODE_CNT;

  // Ordered DF traversal, visit near child first
  loop {
    let node = &nodes[blasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect contained triangle
      let nodeIdx = (*node).idx;
      let tri = tris[dataOfs + nodeIdx];
      let tempHit = intersectTri(ori, dir, tri.v0, tri.v1, tri.v2);
      if(tempHit.x > EPS && tempHit.x < (*hit).x) {
        *hit = vec4f(tempHit, bitcast<f32>((nodeIdx << 16) | (instId & SHORT_MASK)));
      }
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == HALF_MAX_NODE_CNT) {
        return;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
      continue;
    }

    // Interior node
    let leftChildNode = &nodes[blasOfs + (nodeChildren & SHORT_MASK)];
    let rightChildNode = &nodes[blasOfs + (nodeChildren >> 16)];

    // Intersect both child node aabbs
    let childDists = array<f32, 2>(
      intersectAabb(ori, invDir, (*hit).x, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
      intersectAabb(ori, invDir, (*hit).x, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1u, 0u, childDists[0] < childDists[1]);
    let far = 1u - near;

    if(childDists[near] == INF) {
      // Missed both children
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == HALF_MAX_NODE_CNT) {
        return;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    } else {
      // Continue with nearer child node
      nodeIndex = (nodeChildren >> (near << 4u)) & SHORT_MASK;
      // Push farther child on stack if also within distance
      if(childDists[far] < INF) {
        nodeStack[nodeStackIndex] = (nodeChildren >> (far << 4u)) & SHORT_MASK;
        nodeStackIndex++;
      }
    }
  }
}

fn intersectInst(ori: vec3f, dir: vec3f, inst: Inst, hit: ptr<function, vec4f>)
{
  // Transform to object space of the instance
  let m = toMat4x4(inst.invTransform);
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = dir * mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);

  intersectBlas(oriObj, dirObj, 1.0 / dirObj, inst.id, inst.data & MESH_SHAPE_MASK, hit);
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
/*fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> vec4f
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeStackIndex = 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (instance) yet
 
  var hit = vec4f(tfar, 0, 0, 0);

  // Ordered DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {
      // Get current node's child indices
      let nodeChildren = nodes[tlasOfs + u32(nodeIndex)].children;

      // Get child nodes
      let leftChildNode = &nodes[tlasOfs + (nodeChildren & SHORT_MASK)];
      let rightChildNode = &nodes[tlasOfs + (nodeChildren >> 16)];

      // Intersect both child node aabbs
      var childDists = array<f32, 2>(
        intersectAabb(ori, invDir, hit.x, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
        intersectAabb(ori, invDir, hit.x, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

      // Find indices of nearer and farther child
      let near = select(1u, 0u, childDists[0] < childDists[1]);
      let far = 1u - near;

      if(childDists[near] == INF) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children
        let childNodeIdx = array<u32, 2>( (*leftChildNode).idx, (*rightChildNode).idx );
        let childChildren = array<u32, 2>( (*leftChildNode).children, (*rightChildNode).children );

        // Set near child as next node
        // In case of leaf child, set its instance index negated and incremented by one (to account for idx of 0)
        nodeIndex = select(-i32(childNodeIdx[near] + 1), i32((nodeChildren >> (near << 4u)) & SHORT_MASK), childChildren[near] > 0);

        if(childDists[far] < INF) {
          // Push far child on stack. Do the same as above in case this child is a leaf.
          nodeStack[nodeStackIndex] =
            select(-i32(childNodeIdx[far] + 1), i32((nodeChildren >> (far << 4u)) & SHORT_MASK), childChildren[far] > 0);
          nodeStackIndex++;
        }
      }

      // If our next node is a leaf node (as per above leaf nodes have a negative index) and we found no other leaf yet
      if(nodeIndex < 0 && leafIndex >= 0) {
        // Store the found leaf, so we can continue traversal
        leafIndex = nodeIndex;
        // Pop next node
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
        // Another one in the workgroup has found a leaf
        atomicAdd(&foundLeafCnt, 1u);
      }

      // Synchronize: Everyone in the workgroup has found a leaf
      if(atomicLoad(&foundLeafCnt) == WG_SIZE.x * WG_SIZE.y) {
        break;
      }
    }

    // Reset
    atomicStore(&foundLeafCnt, 0u);

    // Instance "traversal"
    while(leafIndex < 0) {
      // Transform leaf index back into the instance index we had stored at the node
      let instIdx = u32(abs(leafIndex + 1));
      intersectInst(ori, dir, instances[instIdx], &hit);
      // Set next potential leaf for processing
      leafIndex = nodeIndex;
      if(leafIndex < 0) {
        // Have another leaf/instance available
        // Pop next node (which could very well be a leaf too)
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    }
  }

  return hit;
}*/

fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> vec4f
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  var hit = vec4f(tfar, 0, 0, 0);

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
    let leftChildNode = &nodes[tlasOfs + (nodeChildren & SHORT_MASK)];
    let rightChildNode = &nodes[tlasOfs + (nodeChildren >> 16)];

    // Intersect both child node aabbs
    let childDists = array<f32, 2>(
      intersectAabb(ori, invDir, hit.x, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax),
      intersectAabb(ori, invDir, hit.x, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax) );

    // Find indices of nearer and farther child
    let near = select(1u, 0u, childDists[0] < childDists[1]);
    let far = 1u - near;

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
      nodeIndex = (nodeChildren >> (near << 4u)) & SHORT_MASK;
      // Push farther child on stack if also within distance
      if(childDists[far] < INF) {
        nodeStack[nodeStackIndex] = (nodeChildren >> (far << 4u)) & SHORT_MASK;
        nodeStackIndex++;
      } 
    }
  }

  return hit; // Required for Naga, Tint will warn on this
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = config.gridDimPath.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= config.pathCnt) {
    return;
  }

  // Initial reset
  /*atomicStore(&foundLeafCnt, 0u);
  
  // Push empty marker on stack, so we know when to stop
  nodeStack[                0] = STACK_EMPTY_MARKER;
  nodeStack[HALF_MAX_NODE_CNT] = STACK_EMPTY_MARKER;
  */

  let ofs = (config.width >> 8) * (config.height >> 8) * (config.samplesTaken & 0xff);
  let pathState = pathStates[ofs + gidx];
  hits[gidx] = intersectTlas(pathState.ori, pathState.dir, INF);
}
