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
  bgColor:          vec4f
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

struct ShadowRay
{
  ori:              vec3f,          // Shadow ray origin
  pidx:             u32,            // Pixel index where to deposit contribution
  dir:              vec3f,          // Position on the light (shadow ray target)
  dist:             f32,
  contribution:     vec3f,
  pad0:             f32
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
@group(0) @binding(4) var<storage, read> shadowRays: array<ShadowRay>;
@group(0) @binding(5) var<storage, read_write> accum: array<vec4f>;

// Traversal stacks
const MAX_NODE_CNT      = 64u;
const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
var<private> nodeStack: array<u32, MAX_NODE_CNT>; // For Aila traversal, switch to i32

// Syncs the workgroup execution
//var<workgroup> foundLeafCnt: atomic<u32>;

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
fn intersectAabbAnyHit(ori: vec3f, invDir: vec3f, tfar: f32, minExt: vec3f, maxExt: vec3f) -> bool
{
  let t0 = (minExt - ori) * invDir;
  let t1 = (maxExt - ori) * invDir;

  return maxComp4(vec4f(min(t0, t1), EPS)) <= minComp4(vec4f(max(t0, t1), tfar));
}

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ori: vec3f, dir: vec3f, tfar: f32, v0: vec3f, v1: vec3f, v2: vec3f) -> bool
{
  // Vectors of two edges sharing vertex 0
  let edge1 = v1 - v0;
  let edge2 = v2 - v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < EPS) {
    // Ray in plane of triangle
    return false;
  }

  let invDet = 1.0 / det;

  // Distance vertex 0 to origin
  let tvec = ori - v0;

  // Calculate parameter u and test bounds
  let u = dot(tvec, pvec) * invDet;
  if(u < 0.0 || u > 1.0) {
    return false;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  let v = dot(dir, qvec) * invDet;
  if(v < 0.0 || u + v > 1.0) {
    return false;
  }

  // Ray intersects, calc distance
  let dist = dot(edge2, qvec) * invDet;
  return dist > EPS && dist < tfar;
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
/*fn intersectBlasAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  let blasOfs = dataOfs << 1;

  var nodeStackIndex = HALF_MAX_NODE_CNT + 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (triangle) yet

  // DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {
      // Get current node's child indices
      let nodeChildren = nodes[blasOfs + u32(nodeIndex)].children;

      // Get child nodes
      let leftChildNode = &nodes[blasOfs + (nodeChildren & SHORT_MASK)];
      let rightChildNode = &nodes[blasOfs + (nodeChildren >> 16)];

      // Intersect both child node aabbs
      let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

      if(!traverseLeft && !traverseRight) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children
        if(traverseLeft) {
          // In case of leaf child, set its triangle index negated and incremented by one (to account for idx of 0)
          nodeIndex = select(-i32((*leftChildNode).idx + 1), i32(nodeChildren & SHORT_MASK), (*leftChildNode).children > 0);
        } else {
          nodeIndex = select(-i32((*rightChildNode).idx + 1), i32(nodeChildren >> 16), (*rightChildNode).children > 0);
        }
        if(traverseLeft && traverseRight) {
          // Push other child on stack
          nodeStack[nodeStackIndex] =
            select(-i32((*rightChildNode).idx + 1), i32(nodeChildren >> 16), (*rightChildNode).children > 0);
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
      if(intersectTri(ori, dir, tfar, tri.v0, tri.v1, tri.v2)) {
        return true;
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

  return false;
}*/

fn intersectBlasAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  let blasOfs = dataOfs << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = HALF_MAX_NODE_CNT;

  // DF traversal
  loop {
    let node = &nodes[blasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect contained triangle
      let nodeIdx = (*node).idx;
      let tri = tris[dataOfs + nodeIdx];
      if(intersectTri(ori, dir, tfar, tri.v0, tri.v1, tri.v2)) {
        return true;
      }
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == HALF_MAX_NODE_CNT) {
        return false;
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
    let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
    let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

    if(!traverseLeft && !traverseRight) {
      // Did not hit any child, pop next node from stack
      if(nodeStackIndex == HALF_MAX_NODE_CNT) {
        return false;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    } else {
      // Hit one or both children
      nodeIndex = select(nodeChildren >> 16, nodeChildren & SHORT_MASK, traverseLeft);
      if(traverseLeft && traverseRight) {
        // Push the other child on the stack
        nodeStack[nodeStackIndex] = nodeChildren >> 16;
        nodeStackIndex++;
      }
    }
  }

  return false; // Required for Naga, Tint will warn on this
}

fn intersectInstAnyHit(ori: vec3f, dir: vec3f, tfar: f32, inst: Inst) -> bool
{
  // Transform to object space of the instance
  let m = toMat4x4(inst.invTransform);
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = dir * mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);

  return intersectBlasAnyHit(oriObj, dirObj, 1.0 / dirObj, tfar, inst.data & MESH_SHAPE_MASK);
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
/*fn intersectTlasAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeStackIndex = 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (instance) yet

  // DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {
      // Get current node's child indices
      let nodeChildren = nodes[tlasOfs + u32(nodeIndex)].children;

      // Get child nodes
      let leftChildNode = &nodes[tlasOfs + (nodeChildren & SHORT_MASK)];
      let rightChildNode = &nodes[tlasOfs + (nodeChildren >> 16)];

      // Intersect both child node aabbs
      let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

      if(!traverseLeft && !traverseRight) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children
        if(traverseLeft) {
          // In case of leaf child, set its triangle index negated and incremented by one (to account for idx of 0)
          nodeIndex = select(-i32((*leftChildNode).idx + 1), i32(nodeChildren & SHORT_MASK), (*leftChildNode).children > 0);
        } else {
          nodeIndex = select(-i32((*rightChildNode).idx + 1), i32(nodeChildren >> 16), (*rightChildNode).children > 0);
        }
        if(traverseLeft && traverseRight) {
          // Push other child on stack
          nodeStack[nodeStackIndex] =
            select(-i32((*rightChildNode).idx + 1), i32(nodeChildren >> 16), (*rightChildNode).children > 0);
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
      if(intersectInstAnyHit(ori, dir, tfar, instances[instIdx])) {
        return true;
      }
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

  return false;
}*/

fn intersectTlasAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let invDir = 1.0 / dir;

  let tlasOfs = arrayLength(&tris) << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  // DF traversal
  loop {
    let node = &nodes[tlasOfs + nodeIndex];
    let nodeChildren = (*node).children;

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect the single assigned instance
      if(intersectInstAnyHit(ori, dir, tfar, instances[(*node).idx])) {
        return true;
      }
      // Check the stack and continue traversal if something left
      if(nodeStackIndex == 0) {
        return false;
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
    let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
    let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);

    if(!traverseLeft && !traverseRight) {
      // Did not hit any child, pop next node from stack
      if(nodeStackIndex == 0) {
        return false;
      } else {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      }
    } else {
      // Hit one or both children
      nodeIndex = select(nodeChildren >> 16, nodeChildren & SHORT_MASK, traverseLeft);
      if(traverseLeft && traverseRight) {
        // Push the other child on the stack
        nodeStack[nodeStackIndex] = nodeChildren >> 16;
        nodeStackIndex++;
      }
    }
  }

  return false; // Required for Naga, Tint will warn on this
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = config.gridDimSRay.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= config.shadowRayCnt) {
    return;
  }

  // Initial reset
  //atomicStore(&foundLeafCnt, 0u);

  // Push empty marker on stack, so we know when to stop
  //nodeStack[                0] = STACK_EMPTY_MARKER;
  //nodeStack[HALF_MAX_NODE_CNT] = STACK_EMPTY_MARKER;

  let shadowRay = shadowRays[gidx];
  if(!intersectTlasAnyHit(shadowRay.ori, shadowRay.dir, shadowRay.dist)) {
    accum[shadowRay.pidx] += vec4f(shadowRay.contribution, 1.0);
  }
}
