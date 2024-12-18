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
const SHORT_MASK          = 0xffffu;
const INST_DATA_MASK      = 0x3fffffffu; // Bits 31-0
const EPS                 = 0.0001;
const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> instances: array<vec4f, 4096>; // Uniform buffer max is 64kb by default
@group(0) @binding(1) var<storage, read> tris: array<vec4f>;
@group(0) @binding(2) var<storage, read> nodes: array<vec4f>;
@group(0) @binding(3) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(4) var<storage, read> shadowRays: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> colBuf: array<vec4f>;

// Traversal stacks
const MAX_NODE_CNT      = 64u;
const HALF_MAX_NODE_CNT = MAX_NODE_CNT / 2u;
var<private> nodeStack: array<u32, MAX_NODE_CNT>;

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
// (slightly rearranged)
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

fn intersectBlasAnyHit(ori: vec3f, dir: vec3f, invDir: vec3f, tfar: f32, dataOfs: u32) -> bool
{
  let blasOfs = dataOfs << 1;

  var nodeIndex = 0u;
  var nodeStackIndex = HALF_MAX_NODE_CNT;

  // DF traversal
  loop {
    var ofs = (blasOfs + nodeIndex) << 1;
    let nodeChildren = bitcast<u32>(nodes[ofs + 0].w);

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect contained triangle
      let triOfs = (dataOfs + bitcast<u32>(nodes[ofs + 1].w)) * 3; // 3 vec4f per tri
      if(intersectTri(ori, dir, tfar, tris[triOfs + 0].xyz, tris[triOfs + 1].xyz, tris[triOfs + 2].xyz)) {
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
    ofs = (blasOfs + (nodeChildren & SHORT_MASK)) << 1;
    let leftChildMin = nodes[ofs + 0];
    let leftChildMax = nodes[ofs + 1];

    ofs = (blasOfs + (nodeChildren >> 16)) << 1;
    let rightChildMin = nodes[ofs + 0];
    let rightChildMax = nodes[ofs + 1];

    // Intersect both child node aabbs
    let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, leftChildMin.xyz, leftChildMax.xyz);
    let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, rightChildMin.xyz, rightChildMax.xyz);

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

fn intersectTlasAnyHit(ori: vec3f, dir: vec3f, tfar: f32) -> bool
{
  let invDir = 1.0 / dir;

  // Skip 2 * tri cnt blas nodes with 3 * vec4f per tri struct
  let tlasOfs = 2 * arrayLength(&tris) / 3;

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  // DF traversal
  loop {
    var ofs = (tlasOfs + nodeIndex) << 1;
    let nodeChildren = bitcast<u32>(nodes[ofs + 0].w);

    // Leaf node
    if(nodeChildren == 0) {
      // Intersect the single assigned instance
      if(intersectInstAnyHit(ori, dir, tfar, bitcast<u32>(nodes[ofs + 1].w) << 2)) {
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
    ofs = (tlasOfs + (nodeChildren & SHORT_MASK)) << 1;
    let leftChildMin = nodes[ofs + 0];
    let leftChildMax = nodes[ofs + 1];

    ofs = (tlasOfs + (nodeChildren >> 16)) << 1;
    let rightChildMin = nodes[ofs + 0];
    let rightChildMax = nodes[ofs + 1];

    // Intersect both child node aabbs
    let traverseLeft = intersectAabbAnyHit(ori, invDir, tfar, leftChildMin.xyz, leftChildMax.xyz);
    let traverseRight = intersectAabbAnyHit(ori, invDir, tfar, rightChildMin.xyz, rightChildMax.xyz);

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
  let shadowRayGrid = config[2];
  let gidx = shadowRayGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= shadowRayGrid.w) { // shadow ray cnt
    return;
  }

  let frame = config[0];
  let ofs = (frame.x >> 16) * (frame.y >> 16); // width * height
  let ori = shadowRays[             gidx];
  let dir = shadowRays[       ofs + gidx];
  let ctb = shadowRays[(ofs << 1) + gidx];
  if(!intersectTlasAnyHit(ori.xyz, dir.xyz, dir.w)) { // dir.w = distance
    colBuf[bitcast<u32>(ori.w)] += vec4f(ctb.xyz, 1.0);
  }
}
