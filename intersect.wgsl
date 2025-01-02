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

/*struct BvhNode
{
  aabbMin:          vec3f,
  children:         u32,            // 2x 16 bits for left and right child
  aabbMax:          vec3f,
  idx:              u32             // Assigned on leaf nodes only
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

/*struct Hit
{
  t:                f32,
  u:                f32,
  v:                f32,
  id:               f32             // tri id << 16 | inst id & 0xffff
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
const IF_INVISIBLE        = 0x1u;

// General constants
const EPS                 = 0.0001;
const INF                 = 3.402823466e+38;

const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> instances: array<vec4f, 4096>; // Uniform buffer max is 64kb by default
@group(0) @binding(1) var<storage, read> tris: array<vec4f>;
@group(0) @binding(2) var<storage, read> nodes: array<vec4f>;
@group(0) @binding(3) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(4) var<storage, read> pathStates: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> hits: array<vec4f>;

// Traversal stacks
//const MAX_NODE_CNT      = 64u;
//const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
//var<private> nodeStack: array<u32, MAX_NODE_CNT>;

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

fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, vec4f>)
{
  //let blasOfs = dataOfs << 1;
  let blasOfs = 6 * 2 * dataOfs;
  var idx = 0u;

  while(idx < SHORT_MASK) { // Not terminal
    var ofs = (blasOfs + idx) << 1;
    // Retrieve node data
    let aabbMin = nodes[ofs + 0]; // w = miss link << 16 | hit link
    let aabbMax = nodes[ofs + 1]; // w = triangle index
    // Test for intersection with bbox
    if(intersectAabbAnyHit(ori, invDir, (*hit).x, aabbMin.xyz, aabbMax.xyz)) {
      // Check if leaf
      let triIdx = bitcast<u32>(aabbMax.w);
      if(triIdx < 0xffffffff) {
        // Intersect contained triangle
        let triOfs = (dataOfs + triIdx) * 3; // 3 vec4f per tri
        intersectTri(ori, dir, tris[triOfs + 0].xyz, tris[triOfs + 1].xyz, tris[triOfs + 2].xyz, (triIdx << 16) | (instId & SHORT_MASK), hit);
      }
      // Follow hit link
      idx = bitcast<u32>(aabbMin.w) & SHORT_MASK;
    } else {
      // Follow miss link
      idx = bitcast<u32>(aabbMin.w) >> 16;
    }
  }
}

/*fn intersectBlas(ori: vec3f, dir: vec3f, invDir: vec3f, instId: u32, dataOfs: u32, hit: ptr<function, vec4f>)
{
  let blasOfs = dataOfs << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = HALF_MAX_NODE_CNT;

  // Ordered DF traversal, visit near child first
  loop {
    var ofs = (blasOfs + nodeIndex) << 1;
    let nodeChildren = bitcast<u32>(nodes[ofs + 0].w);

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect contained triangle
      let idx = bitcast<u32>(nodes[ofs + 1].w);
      let triOfs = (dataOfs + idx) * 3; // 3 vec4f per tri
      intersectTri(ori, dir, tris[triOfs + 0].xyz, tris[triOfs + 1].xyz, tris[triOfs + 2].xyz, (idx << 16) | (instId & SHORT_MASK), hit);
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
    ofs = (blasOfs + (nodeChildren & SHORT_MASK)) << 1;
    let leftChildMin = nodes[ofs + 0];
    let leftChildMax = nodes[ofs + 1];

    ofs = (blasOfs + (nodeChildren >> 16)) << 1;
    let rightChildMin = nodes[ofs + 0];
    let rightChildMax = nodes[ofs + 1];

    // Intersect both child node aabbs
    var childDists: array<f32, 2u> = array<f32, 2u>(
      intersectAabb(ori, invDir, (*hit).x, leftChildMin.xyz, leftChildMax.xyz),
      intersectAabb(ori, invDir, (*hit).x, rightChildMin.xyz, rightChildMax.xyz) );

    // Find indices of nearer and farther child
    let near = select(1u, 0u, childDists[0] < childDists[1]);
    let far = 1u - near;

    if(childDists[near] == INF) {
      // Did not hit any child, pop next node from stack
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
}*/

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

fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> vec4f
{
  let invDir = 1.0 / dir;

  // Skip 6 * 2 * tri cnt blas nodes with 3 * vec4f per tri struct
  let tlasOfs = 6 * 2 * arrayLength(&tris) / 3;

  var hit = vec4f(tfar, 0, 0, 0);

  var idx = 0u;
  
  while(idx < SHORT_MASK) { // Not terminal
    var ofs = (tlasOfs + idx) << 1;
    // Retrieve node data
    let aabbMin = nodes[ofs + 0]; // w = miss link << 16 | hit link
    let aabbMax = nodes[ofs + 1]; // w = instance index
    // Test for intersection with bbox
    if(intersectAabbAnyHit(ori, invDir, hit.x, aabbMin.xyz, aabbMax.xyz)) {
      // Check if leaf
      let instIdx = bitcast<u32>(aabbMax.w);
      if(instIdx < 0xffffffff) {
        // Intersect referenced instance 
        intersectInst(ori, dir, instIdx << 2, &hit);
      }
      // Follow hit link
      idx = bitcast<u32>(aabbMin.w) & SHORT_MASK;
    } else {
      // Follow miss link
      idx = bitcast<u32>(aabbMin.w) >> 16;
    }
  }

  return hit;
}

/*fn intersectTlas(ori: vec3f, dir: vec3f, tfar: f32) -> vec4f
{
  let invDir = 1.0 / dir;

  // Skip 2 * tri cnt blas nodes with 3 * vec4f per tri struct
  let tlasOfs = 2 * arrayLength(&tris) / 3;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  var hit = vec4f(tfar, 0, 0, 0);

  // Ordered DF traversal, visit near child first
  loop {
    var ofs = (tlasOfs + nodeIndex) << 1;
    let nodeChildren = bitcast<u32>(nodes[ofs + 0].w);

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect the single assigned instance 
      intersectInst(ori, dir, bitcast<u32>(nodes[ofs + 1].w) << 2, &hit);
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
    ofs = (tlasOfs + (nodeChildren & SHORT_MASK)) << 1;
    let leftChildMin = nodes[ofs + 0];
    let leftChildMax = nodes[ofs + 1];

    ofs = (tlasOfs + (nodeChildren >> 16)) << 1;
    let rightChildMin = nodes[ofs + 0];
    let rightChildMax = nodes[ofs + 1];

    // Intersect both child node aabbs
    var childDists: array<f32, 2u> = array<f32, 2>(
      intersectAabb(ori, invDir, hit.x, leftChildMin.xyz, leftChildMax.xyz),
      intersectAabb(ori, invDir, hit.x, rightChildMin.xyz, rightChildMax.xyz) );

    // Find indices of nearer and farther child
    let near = select(1u, 0u, childDists[0] < childDists[1]);
    let far = 1u - near;

    if(childDists[near] == INF) {
      // Did not hit any child, pop next node from stack
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
}*/

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let pathStateGrid = config[1];
  let gidx = pathStateGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= pathStateGrid.w) { // path state cnt
    return;
  }

  let frame = config[0];
  let ofs = (frame.x >> 16) * (frame.y >> 16);
  hits[gidx] = intersectTlas(pathStates[ofs + gidx].xyz, pathStates[(ofs << 1) + gidx].xyz, INF);
}
