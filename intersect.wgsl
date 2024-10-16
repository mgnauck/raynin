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

/*struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, flags in bits 4-7, bounce num in bits 0-3
}*/

// Scene data handling
const SHORT_MASK          = 0xffffu;
const INST_DATA_MASK      = 0x7fffffffu; // Bits 31-0

// Instance flags (see inst_flags)
const IF_INVISIBLE        = 0x1;

// General constants
const EPS                 = 0.0001;
const INF                 = 3.402823466e+38;

const STACK_EMPTY_MARKER  = 0xfffffffi;

const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> instances: array<vec4f, 1024 * 4>; // Uniform buffer max is 64kb by default
@group(0) @binding(1) var<storage, read> tris: array<vec4f>;
@group(0) @binding(2) var<storage, read> nodes: array<vec4f>;
@group(0) @binding(3) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(4) var<storage, read> pathStates: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> hits: array<vec4f>;

// Traversal stacks
const MAX_NODE_CNT      = 64u;
const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
var<private> nodeStack: array<i32, MAX_NODE_CNT>; // For Aila traversal, switch to i32

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
// (slightly rearranged)
fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f, instTriId: u32, hit: ptr<function, vec4f>)
{
  let edge0 = v1 - v0;
  let edge1 = v2 - v0;

  let pvec = cross(dir, edge1);
  let det = dot(edge0, pvec);
  let tvec = ori - v0;
  let qvec = cross(tvec, edge0);

  var uvt = vec4f(dot(tvec, pvec), dot(dir, qvec), dot(edge1, qvec), 0.0) / det;
  uvt.w = 1.0 - uvt.x - uvt.y;

  *hit = select(*hit, vec4f(uvt.z, uvt.xy, bitcast<f32>(instTriId)), all(uvt.xyw >= vec3f(0.0)) && uvt.z > EPS && uvt.z < (*hit).x);
}

// Moeller/Trumbore: Ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
/*fn intersectTri(ori: vec3f, dir: vec3f, v0: vec3f, v1: vec3f, v2: vec3f, instTriId: u32, hit: ptr<function, vec4f>)
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
  let t = dot(edge2, qvec) * invDet;
  if(t > EPS && t < (*hit).x) {
    *hit = vec4f(t, u, v, bitcast<f32>(instTriId));
  }
}*/

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, vec4f>)
{
  let blasOfs = dataOfs << 1;

  var nodeStackIndex = HALF_MAX_NODE_CNT + 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (triangle) yet

  // Ordered DF traversal, with postponed leaf node processing for improved WG sync
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
      var dists = array<f32, 2>(
        intersectAabb(ori, invDir, (*hit).x, lmin.xyz, lmax.xyz),
        intersectAabb(ori, invDir, (*hit).x, rmin.xyz, rmax.xyz) );

      // Find indices of nearer and farther child
      let near = select(1u, 0u, dists[0] < dists[1]);
      let far = 1u - near;

      if(dists[near] == INF) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children, set near child as next node
        let children = array<i32, 2>( bitcast<i32>(lmin.w), bitcast<i32>(rmin.w) );
        nodeIndex = children[near];
        if(dists[far] < INF) {
          // Push far child on stack
          nodeStack[nodeStackIndex] = children[far];
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
      // Transform negated leaf index back into an actual triangle index
      let triIdx = u32(~leafIndex);
      // Fetch tri data and intersect
      let triOfs = (dataOfs + triIdx) * 3; // 3 vec4f per tri
      intersectTri(ori, dir, tris[triOfs + 0].xyz, tris[triOfs + 1].xyz, tris[triOfs + 2].xyz, (triIdx << 16) | (instId & SHORT_MASK), hit);
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
}

fn intersectInst(ori: vec3f, dir: vec3f, instOfs: u32, hit: ptr<function, vec4f>)
{
  // Inst id, inst data, inst flags
  let data = instances[instOfs + 3];

  // Do not intersect further if instance is invisible anyway
  if((bitcast<u32>(data.z) & IF_INVISIBLE) > 0) {
    return;
  }

  // Inst inverse transform
  let m = mat4x4f(  instances[instOfs + 0],
                    instances[instOfs + 1],
                    instances[instOfs + 2],
                    vec4f(0.0, 0.0, 0.0, 1.0));

  // Transform ray to inst object space
  let oriObj = (vec4f(ori, 1.0) * m).xyz;
  let dirObj = (vec4f(dir, 0.0) * m).xyz;

  intersectBlas(oriObj, dirObj, 1.0 / dirObj, bitcast<u32>(data.x), bitcast<u32>(data.y) & INST_DATA_MASK, hit);
}

// Aila et al: Understanding the Efficiency of Ray Traversal on GPUs
// https://code.google.com/archive/p/understanding-the-efficiency-of-ray-traversal-on-gpus/
// Karras: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> vec4f
{
  let invDir = 1.0 / dir;

  let tlasOfs = 2 * arrayLength(&tris) / 3; // = skip 2 * tri cnt blas nodes with 3 * vec4f per tri struct

  var nodeStackIndex = 1u;

  var nodeIndex = 0i; // Start at root node, but do not test its AABB
  var leafIndex = 0i; // No leaf node (instance) yet
 
  var hit = vec4f(tfar, 0, 0, 0);

  // Ordered DF traversal, with postponed leaf node processing for improved WG sync
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
      var dists = array<f32, 2>(
        intersectAabb(ori, invDir, hit.x, lmin.xyz, lmax.xyz),
        intersectAabb(ori, invDir, hit.x, rmin.xyz, rmax.xyz) );

      // Find indices of nearer and farther child
      let near = select(1u, 0u, dists[0] < dists[1]);
      let far = 1u - near;

      if(dists[near] == INF) {
        // Did not hit any child, pop next node from stack
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        // Hit one or both children, set near child as next node
        let children = array<i32, 2>( bitcast<i32>(lmin.w), bitcast<i32>(rmin.w) );
        nodeIndex = children[near]; 
        if(dists[far] < INF) {
          // Push far child on stack
          nodeStack[nodeStackIndex] = children[far];
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
      // Transform negated leaf index back into actual instance index and intersect
      intersectInst(ori, dir, u32(~leafIndex) << 2, &hit);
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
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let pathStateGrid = config[1];
  let gidx = pathStateGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= pathStateGrid.w) { // path state cnt
    return;
  }

  let frame = config[0];

  // Initial reset
  //atomicStore(&foundLeafCnt0, 0u);
  //atomicStore(&foundLeafCnt1, 0u);
  
  // Push empty marker on stack, so we know when to stop
  nodeStack[                0] = STACK_EMPTY_MARKER;
  nodeStack[HALF_MAX_NODE_CNT] = STACK_EMPTY_MARKER;

  let ofs = (frame.x >> 16) * (frame.y >> 16);
  hits[gidx] = intersectTlas(pathStates[ofs + gidx].xyz, pathStates[(ofs << 1) + gidx].xyz, INF);
}
