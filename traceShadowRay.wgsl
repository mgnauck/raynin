/*struct Config
{
  frameData:        vec4u,          // x = bits 16-31 for width, bits 0-15 for ltri cnt
                                    // y = bits 16-31 for height, bits 4-15 unused, bits 0-3 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

/*struct Inst
{
  invTransform:     mat3x4f,
  id:               u32,            // (mtl override id << 16) | (inst id & 0xffff)
  data:             u32,            // See comment on data in inst.h
  flags:            u32,            // Inst flags
  pad1:             u32
}*/

/*struct Node
{
  lmin:             vec3f,
  left:             i32,
  lmax:             vec3f,
  pad0:             u32,
  rmin:             vec3f,
  right:            i32,
  rmax:             vec3f,
  pad1:             u32
}*/

/*struct Tri
{
  v0:               vec3f,
  pad0:             f32,
  v1:               vec3f,
  pad1:             f32,
  v2:               vec3f,
  pad2:             f32,
}*/

/*struct ShadowRay
{
  ori:              vec3f,          // Shadow ray origin
  pidx:             u32,            // Pixel index where to deposit contribution
  dir:              vec3f,          // Position on the light (shadow ray target)
  dist:             f32,
  contribution:     vec3f,
  pad0:             f32
}*/

// Scene data handling
const INST_DATA_MASK      = 0x3fffffffu; // Bits 31-0
const EPS                 = 0.0001;
const STACK_EMPTY_MARKER  = 0xfffffffi;
const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> instances: array<vec4f, 1024 * 4>; // Uniform buffer max is 64kb by default
@group(0) @binding(1) var<storage, read> tris: array<vec4f>;
@group(0) @binding(2) var<storage, read> nodes: array<vec4f>;
@group(0) @binding(3) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(4) var<storage, read> shadowRays: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> colBuf: array<vec4f>;

// Traversal stacks
const MAX_NODE_CNT      = 64u;
const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
var<private> nodeStack: array<i32, MAX_NODE_CNT>;

// Syncs the workgroup execution
//var<workgroup> foundLeafCnt0: atomic<u32>;
//var<workgroup> foundLeafCnt1: atomic<u32>;

fn minComp4(v: vec4f) -> f32
{
  return min(v.x, min(v.y, min(v.z, v.w)));
}

fn maxComp4(v: vec4f) -> f32
{
  return max(v.x, max(v.y, max(v.z, v.w)));
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
// (slightlyt rearranged)
fn intersectTri(ori: vec3f, dir: vec3f, tfar: f32, v0: vec3f, v1: vec3f, v2: vec3f) -> bool
{
  let edge0 = v1 - v0;
  let edge1 = v2 - v0;

  let pvec = cross(dir, edge1);
  let det = dot(edge0, pvec);
  let tvec = ori - v0;
  let qvec = cross(tvec, edge0);

  var uvt = vec4f(dot(tvec, pvec), dot(dir, qvec), dot(edge1, qvec), 0.0) / det;
  uvt.w = 1.0 - uvt.x - uvt.y;

  return select(false, true, all(uvt.xyw >= vec3f(0.0)) && uvt.z > EPS && uvt.z < tfar);
}

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
/*fn intersectTri(ori: vec3f, dir: vec3f, tfar: f32, v0: vec3f, v1: vec3f, v2: vec3f) -> bool
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
  let t = dot(edge2, qvec) * invDet;
  return select(false, true, t > EPS && t < tfar);
}*/

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
fn intersectBlasAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  let blasOfs = dataOfs << 1;

  var nodeStackIndex = HALF_MAX_NODE_CNT + 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (triangle) yet

  // DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {

      // Get current node
      let ofs = (blasOfs + u32(nodeIndex)) << 2;
      let lmin = nodes[ofs + 0];
      let lmax = nodes[ofs + 1];
      let rmin = nodes[ofs + 2];
      let rmax = nodes[ofs + 3];

      // Intersect both child node aabbs
      let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, lmin.xyz, lmax.xyz);
      let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, rmin.xyz, rmax.xyz);

      if(!traverseLeft && !traverseRight) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children, set left child as next node
        nodeIndex = select(bitcast<i32>(rmin.w), bitcast<i32>(lmin.w), traverseLeft);
        if(traverseLeft && traverseRight) {
          // Push right child on stack
          nodeStack[nodeStackIndex] = bitcast<i32>(rmin.w);
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
        //atomicAdd(&foundLeafCnt1, 1u);
      }

      // Synchronize: Everyone in the workgroup has found a leaf
      //if(atomicLoad(&foundLeafCnt1) == WG_SIZE.x * WG_SIZE.y) {
      //  break;
      //}
    }

    // Reset
    //atomicStore(&foundLeafCnt1, 0u);

    // Triangle "traversal"
    while(leafIndex < 0) {
      // Transform negated leaf index back into actual triangle index
      let triOfs = (dataOfs + u32(~leafIndex)) * 3; // 3 * vec4f per tri
      if(intersectTri(ori, dir, tfar, tris[triOfs].xyz, tris[triOfs + 1].xyz, tris[triOfs + 2].xyz)) {
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
}

fn intersectInstAnyHit(ori: vec3f, dir: vec3f, tfar: f32, instOfs: u32) -> bool
{
  // Inst inverse transform
  let m = mat4x4f(  instances[instOfs + 0],
                    instances[instOfs + 1],
                    instances[instOfs + 2],
                    vec4f(0.0, 0.0, 0.0, 1.0));

  // Inst id + inst data
  let data = instances[instOfs + 3];

  // Transform ray to inst object space
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = (vec4f(dir, 0.0) * m).xyz;

  return intersectBlasAnyHit(oriObj, dirObj, 1.0 / dirObj, tfar, bitcast<u32>(data.y) & INST_DATA_MASK);
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
fn intersectTlasAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let invDir = 1.0 / dir;

  let tlasOfs = 2 * arrayLength(&tris) / 3; // = skip 2 * tri cnt blas nodes with 3 * vec4f per tri struct

  var nodeStackIndex = 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (instance) yet

  // DF traversal, with postponed leaf node processing for improved WG sync
  while(nodeIndex != STACK_EMPTY_MARKER) {

    // Interior node traversal
    while(nodeIndex >= 0 && nodeIndex != STACK_EMPTY_MARKER) {

      // Get current node
      let ofs = (tlasOfs + u32(nodeIndex)) << 2;
      let lmin = nodes[ofs + 0];
      let lmax = nodes[ofs + 1];
      let rmin = nodes[ofs + 2];
      let rmax = nodes[ofs + 3];

      // Intersect both child node aabbs
      let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, lmin.xyz, lmax.xyz);
      let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, rmin.xyz, rmax.xyz);

      if(!traverseLeft && !traverseRight) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children, set left child as next node
        nodeIndex = select(bitcast<i32>(rmin.w), bitcast<i32>(lmin.w), traverseLeft);
        if(traverseLeft && traverseRight) {
          // Push right child on stack
          nodeStack[nodeStackIndex] = bitcast<i32>(rmin.w);
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
        //atomicAdd(&foundLeafCnt0, 1u);
      }

      // Synchronize: Everyone in the workgroup has found a leaf
      //if(atomicLoad(&foundLeafCnt0) == WG_SIZE.x * WG_SIZE.y) {
      //  break;
      //}
    }

    // Reset
    //atomicStore(&foundLeafCnt0, 0u);

    // Instance "traversal"
    while(leafIndex < 0) {
      // Transform negated leaf index back into the actual instance index
      if(intersectInstAnyHit(ori, dir, tfar, u32(~leafIndex) << 2)) {
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
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let shadowRayGrid = config[2];
  let gidx = shadowRayGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= shadowRayGrid.w) { // shadow ray cnt
    return;
  }

  // Initial reset
  //atomicStore(&foundLeafCnt0, 0u);
  //atomicStore(&foundLeafCnt1, 0u);

  // Push empty marker on stack, so we know when to stop
  nodeStack[                0] = STACK_EMPTY_MARKER;
  nodeStack[HALF_MAX_NODE_CNT] = STACK_EMPTY_MARKER;

  let frame = config[0];

  let ofs = (frame.x >> 16) * (frame.y >> 16); // width * height
  let ori = shadowRays[             gidx];
  let dir = shadowRays[       ofs + gidx];
  let ctb = shadowRays[(ofs << 1) + gidx];
  if(!intersectTlasAnyHit(ori.xyz, dir.xyz, dir.w)) { // dir.w = distance
    colBuf[bitcast<u32>(ori.w)] += vec4f(ctb.xyz, 1.0);
  }
}
