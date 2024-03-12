struct Global
{
  width: u32,
  height: u32,
  samplesPerPixel: u32,
  maxBounces: u32,
  rngSeed: f32,
  weight: f32,
  time: f32,
  pad1: f32,
  bgColor: vec3f,
  pad2: f32,
  eye: vec3f,
  vertFov: f32,
  right: vec3f,
  focDist: f32,
  up: vec3f,
  focAngle: f32,
  pixelDeltaX: vec3f,
  pad3: f32,
  pixelDeltaY: vec3f,
  pad4: f32,
  pixelTopLeft: vec3f,
  pad5: f32
}

struct Ray
{
  ori: vec3f,
  dir: vec3f,
  invDir: vec3f,
}

struct Tri
{
  v0:   vec3f,
  mtl:  u32,        // (mtl flags << 16) | (mtl id & 0xffff)
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

struct BvhNode
{
  aabbMin: vec3f,
  startIndex: u32,  // Either index of first object or left child node
  aabbMax: vec3f,
  objCount: u32
}

struct TlasNode
{
  aabbMin: vec3f,
  children: u32,    // 2x 16 bits
  aabbMax: vec3f,
  inst: u32         // Assigned on leaf nodes only
}

struct Inst
{
  transform: mat4x4f,
  invTransform: mat4x4f,
  aabbMin: vec3f,
  id: u32,          // (mtl override id << 16) | (inst id & 0xffff)
  aabbMax: vec3f,
  data: u32         // See comment on data in inst.h
}

struct Mtl
{
  color: vec3f,
  value: f32
}

struct Hit
{
  t: f32,
  u: f32,
  v: f32,
  e: u32,           // (tri id << 16) | (inst id & 0xffff)
}

// Bit handling
const INST_ID_MASK      = 0xffffu;
const MTL_ID_MASK       = 0xffffu;
const TLAS_NODE_MASK    = 0xffffu;

const SHAPE_TYPE_BIT    = 0x40000000u;
const MESH_SHAPE_MASK   = 0x3fffffffu;
const MTL_OVERRIDE_BIT  = 0x80000000u;

// Shape types
const ST_BOX            = 0u;
const ST_SPHERE         = 1u;
const ST_CYLINDER       = 2u;

// Material flags
const FLAT              = 1u;

// Math constants
const EPSILON           = 0.0001;
const PI                = 3.141592;
const MAX_DISTANCE      = 3.402823466e+38;

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<storage, read> tris: array<Tri>;
@group(0) @binding(2) var<storage, read> indices: array<u32>;
@group(0) @binding(3) var<storage, read> bvhNodes: array<BvhNode>;
@group(0) @binding(4) var<storage, read> tlasNodes: array<TlasNode>;
@group(0) @binding(5) var<storage, read> instances: array<Inst>;
@group(0) @binding(6) var<storage, read> materials: array<Mtl>;
@group(0) @binding(7) var<storage, read_write> buffer: array<vec4f>;

var<private> bvhNodeStack: array<u32, 32>;
var<private> tlasNodeStack: array<u32, 32>;
var<private> rngState: u32;

// PCG from https://jcgt.org/published/0009/03/02/
fn rand() -> f32
{
  rngState = rngState * 747796405u + 2891336453u;
  let word = ((rngState >> ((rngState >> 28u) + 4u)) ^ rngState) * 277803737u;
  return f32((word >> 22u) ^ word) / f32(0xffffffffu);
}

fn randRange(valueMin: f32, valueMax: f32) -> f32
{
  return mix(valueMin, valueMax, rand());
}

fn rand3() -> vec3f
{
  return vec3f(rand(), rand(), rand());
}

fn rand3Range(valueMin: f32, valueMax: f32) -> vec3f
{
  return vec3f(randRange(valueMin, valueMax), randRange(valueMin, valueMax), randRange(valueMin, valueMax));
}

// https://mathworld.wolfram.com/SpherePointPicking.html
fn rand3UnitSphere() -> vec3f
{
  let u = 2 * rand() - 1;
  let theta = 2 * PI * rand();
  let r = sqrt(1 - u * u);
  return vec3f(r * cos(theta), r * sin(theta), u);
}

fn rand3Hemi(nrm: vec3f) -> vec3f
{
  let v = rand3UnitSphere();
  return select(-v, v, dot(v, nrm) > 0);
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn rand2Disk() -> vec2f
{
  let r = sqrt(rand());
  let theta = 2 * PI * rand();
  return vec2f(r * cos(theta), r * sin(theta));
}

fn minComp(v: vec3f) -> f32
{
  return min(v.x, min(v.y, v.z));
}

fn maxComp(v: vec3f) -> f32
{
  return max(v.x, max(v.y, v.z));
}

fn createRay(ori: vec3f, dir: vec3f) -> Ray
{
  var r: Ray;
  r.ori = ori;
  r.dir = dir;
  r.invDir = 1 / r.dir;
  return r;
}

// GPU efficient slabs test [Laine et al. 2013; Afra et al. 2016]
// https://www.jcgt.org/published/0007/03/04/paper-lowres.pdf
fn intersectAabb(ray: Ray, currT: f32, minExt: vec3f, maxExt: vec3f) -> f32
{
  let t0 = (minExt - ray.ori) * ray.invDir;
  let t1 = (maxExt - ray.ori) * ray.invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t0, t1));
  
  return select(MAX_DISTANCE, tmin, tmin <= tmax && tmin < currT && tmax > EPSILON);
}

fn intersectUnitBox(ray: Ray, instId: u32, h: ptr<function, Hit>)
{
  let t0 = (vec3f(-1.0) - ray.ori) * ray.invDir;
  let t1 = (vec3f( 1.0) - ray.ori) * ray.invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t0, t1));
 
  if(tmin <= tmax) {
    if(tmin < (*h).t && tmin > EPSILON) {
      (*h).t = tmin;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
      return;
    }
    if(tmax < (*h).t && tmax > EPSILON) {
      (*h).t = tmax;
      (*h).e = (ST_BOX << 16) | (instId & INST_ID_MASK);
    }
  }
}

fn intersectUnitSphere(ray: Ray, instId: u32, h: ptr<function, Hit>)
{
  let a = dot(ray.dir, ray.dir);
  let b = dot(ray.ori, ray.dir);
  let c = dot(ray.ori, ray.ori) - 1.0;

  var d = b * b - a * c;
  if(d < 0.0) {
    return;
  }

  d = sqrt(d);
  var t = (-b - d) / a;
  if(t <= EPSILON || (*h).t <= t) {
    t = (-b + d) / a;
    if(t <= EPSILON || (*h).t <= t) {
      return;
    }
  }

  (*h).t = t;
  (*h).e = (ST_SPHERE << 16) | (instId & INST_ID_MASK);
}

fn intersectUnitCylinder(ray: Ray, instId: u32, h: ptr<function, Hit>)
{
  // TODO
}

// Moeller/Trumbore ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ray: Ray, tri: Tri, instId: u32, triId: u32, h: ptr<function, Hit>)
{
  // Vectors of two edges sharing vertex 0
  let edge1 = tri.v1 - tri.v0;
  let edge2 = tri.v2 - tri.v0;

  // Calculate determinant and u parameter later on
  let pvec = cross(ray.dir, edge2);
  let det = dot(edge1, pvec);

  if(abs(det) < EPSILON) {
    // Ray in plane of triangle
    return;
  }

  let invDet = 1 / det;

  // Distance vertex 0 to origin
  let tvec = ray.ori - tri.v0;

  // Calculate parameter u and test bounds
  let u = dot(tvec, pvec) * invDet;
  if(u < 0 || u > 1) {
    return;
  }

  // Prepare to test for v
  let qvec = cross(tvec, edge1);

  // Calculate parameter u and test bounds
  let v = dot(ray.dir, qvec) * invDet;
  if(v < 0 || u + v > 1) {
    return;
  }

  // Calc distance
  let dist = dot(edge2, qvec) * invDet;
  if(dist > EPSILON && dist < (*h).t) {
    (*h).t = dist;
    (*h).u = u;
    (*h).v = v;
    (*h).e = (triId << 16) | (instId & INST_ID_MASK);
  }
}

fn intersectBvh(ray: Ray, instId: u32, dataOfs: u32, h: ptr<function, Hit>)
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  let bvhOfs = 2 * dataOfs;

  loop {
    let node = &bvhNodes[bvhOfs + nodeIndex];
    let nodeStartIndex = (*node).startIndex;
    let nodeObjCount = (*node).objCount;
   
    if(nodeObjCount > 0) {
      // Leaf node, check all contained triangles
      for(var i=0u; i<nodeObjCount; i++) {
        let triId = indices[dataOfs + nodeStartIndex + i];
        intersectTri(ray, tris[dataOfs + triId], instId, triId, h);
      }
      if(nodeStackIndex > 0) {
        nodeStackIndex--;
        nodeIndex = bvhNodeStack[nodeStackIndex];
      } else {
        break;
      }
    } else {
      // Interior node, check aabbs of children
      let leftChildNode = &bvhNodes[bvhOfs + nodeStartIndex];
      let rightChildNode = &bvhNodes[bvhOfs + nodeStartIndex + 1];

      let leftDist = intersectAabb(ray, (*h).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      let rightDist = intersectAabb(ray, (*h).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
      // Swap for nearer child
      let switchNodes = leftDist > rightDist;
      let nearNodeDist = select(leftDist, rightDist, switchNodes);
      let farNodeDist = select(rightDist, leftDist, switchNodes);
      let nearNodeIndex = select(nodeStartIndex, nodeStartIndex + 1, switchNodes);
      let farNodeIndex = select(nodeStartIndex + 1, nodeStartIndex, switchNodes);

      if(nearNodeDist < MAX_DISTANCE) {
        // Continue with nearer child node
        nodeIndex = nearNodeIndex;
        if(farNodeDist < MAX_DISTANCE) {
          // Push farther child on stack if also a hit
          bvhNodeStack[nodeStackIndex] = farNodeIndex;
          nodeStackIndex++;
        }
      } else {
        // Did miss both children, so check stack
        if(nodeStackIndex > 0) {
          nodeStackIndex--;
          nodeIndex = bvhNodeStack[nodeStackIndex];
        } else {
          break;
        }
      }
    }
  }
}

fn intersectInst(ray: Ray, inst: ptr<storage, Inst>, h: ptr<function, Hit>)
{
  // Transform ray into object space of the instance
  var rayObjSpace: Ray;
  rayObjSpace.ori = (vec4f(ray.ori, 1.0) * inst.invTransform).xyz;
  rayObjSpace.dir = (vec4f(ray.dir, 0.0) * inst.invTransform).xyz;
  rayObjSpace.invDir = 1.0 / rayObjSpace.dir;

  if(((*inst).data & SHAPE_TYPE_BIT) > 0) {
    // Shape type
    switch((*inst).data & MESH_SHAPE_MASK) {
      case ST_SPHERE: {
        intersectUnitSphere(rayObjSpace, (*inst).id, h);
      }
      case ST_CYLINDER: {
        intersectUnitCylinder(rayObjSpace, (*inst).id, h);
        return;
      }
      case ST_BOX: {
        intersectUnitBox(rayObjSpace, (*inst).id, h);
      }
      default: {
        return;
      }
    }
  } else {
    // Mesh type
    intersectBvh(rayObjSpace, (*inst).id, (*inst).data & MESH_SHAPE_MASK, h);
  }
}

fn intersectTlas(ray: Ray, h: ptr<function, Hit>)
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  loop {
    let node = &tlasNodes[nodeIndex];
    let nodeChildren = (*node).children;
   
    if(nodeChildren == 0) {
      // Leaf node with a single instance assigned 
      intersectInst(ray, &instances[(*node).inst], h);
      if(nodeStackIndex > 0) {
        nodeStackIndex--;
        nodeIndex = tlasNodeStack[nodeStackIndex];
      } else {
        break;
      }
    } else {
      // Interior node, check aabbs of children
      let leftChildIndex = nodeChildren & TLAS_NODE_MASK;
      let rightChildIndex = nodeChildren >> 16;

      let leftChildNode = &tlasNodes[leftChildIndex];
      let rightChildNode = &tlasNodes[rightChildIndex];

      let leftDist = intersectAabb(ray, (*h).t, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      let rightDist = intersectAabb(ray, (*h).t, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
      // Swap for nearer child
      let switchNodes = leftDist > rightDist;
      let nearNodeDist = select(leftDist, rightDist, switchNodes);
      let farNodeDist = select(rightDist, leftDist, switchNodes);
      let nearNodeIndex = select(leftChildIndex, rightChildIndex, switchNodes);
      let farNodeIndex = select(rightChildIndex, leftChildIndex, switchNodes);

      if(nearNodeDist < MAX_DISTANCE) {
        // Continue with nearer child node
        nodeIndex = nearNodeIndex;
        if(farNodeDist < MAX_DISTANCE) {
          // Push farther child on stack if also a hit
          tlasNodeStack[nodeStackIndex] = farNodeIndex;
          nodeStackIndex++;
        }
      } else {
        // Did miss both children, so check stack
        if(nodeStackIndex > 0) {
          nodeStackIndex--;
          nodeIndex = tlasNodeStack[nodeStackIndex];
        } else {
          break;
        }
      }
    }
  }
}

fn evalMaterialLambert(r: Ray, nrm: vec3f, albedo: vec3f, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let dir = nrm + rand3UnitSphere();
  *scatterDir = select(normalize(dir), nrm, all(abs(dir) < vec3f(EPSILON)));
  *attenuation = albedo;
  return true;
}

fn evalMaterialMetal(r: Ray, nrm: vec3f, albedo: vec3f, fuzzRadius: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let dir = reflect(r.dir, nrm);
  *scatterDir = normalize(dir + fuzzRadius * rand3UnitSphere());
  *attenuation = albedo;
  return dot(*scatterDir, nrm) > 0;
}

fn schlickReflectance(cosTheta: f32, refractionIndexRatio: f32) -> f32
{
  var r0 = (1 - refractionIndexRatio) / (1 + refractionIndexRatio);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow(1 - cosTheta, 5);
}

fn evalMaterialDielectric(r: Ray, nrm: vec3f, inside: bool, albedo: vec3f, refractionIndex: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let refracIndexRatio = select(1 / refractionIndex, refractionIndex, inside);
  
  let cosTheta = min(dot(-r.dir, nrm), 1);
  //let sinTheta = sqrt(1 - cosTheta * cosTheta);
  //
  //var dir: vec3f;
  //if(refracIndexRatio * sinTheta > 1 || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
  //  dir = reflect(r.dir, nrm);
  //} else {
  //  dir = refract(r.dir, nrm, refracIndexRatio);
  //}

  var dir = refract(r.dir, nrm, refracIndexRatio);
  if(all(dir == vec3f(0)) || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
    dir = reflect(r.dir, nrm);
  }

  *scatterDir = dir;
  *attenuation = albedo;
  return true;
}

fn evalMaterial(r: Ray, h: Hit, attenuation: ptr<function, vec3f>, emission: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let inst = &instances[h.e & INST_ID_MASK];

  var mtlId: u32;
  var nrm: vec3f;

  if(((*inst).data & SHAPE_TYPE_BIT) > 0) {
    // For shapes we simply the override material id from the instance
    mtlId = (*inst).id >> 16;
    // Calculate normal for different shapes
    switch((*inst).data & MESH_SHAPE_MASK) {
      case ST_BOX: {
        var pos = r.ori + h.t * r.dir;
        pos = (vec4f(pos, 1.0) * (*inst).invTransform).xyz;
        nrm = pos * step(vec3f(1.0) - abs(pos), vec3f(EPSILON));
        nrm = normalize((vec4f(nrm, 0.0) * (*inst).transform).xyz);
      }
      case ST_SPHERE: {
        let pos = r.ori + h.t * r.dir;
        let m = (*inst).transform;
        nrm = normalize(pos - vec3f(m[0][3], m[1][3], m[2][3]));
      }
      case ST_CYLINDER: {
        // TODO
        nrm = vec3f(0.0);
      }
      default: {
        // Error
        nrm = vec3f(0.0);
      }
    }
  } else {
    // Mesh
    let ofs = (*inst).data & MESH_SHAPE_MASK;
    let tri = &tris[ofs + (h.e >> 16)];

    // Either use the material id from the triangle or the material override from the instance
    mtlId = select((*tri).mtl & MTL_ID_MASK, (*inst).id >> 16, ((*inst).data & MTL_OVERRIDE_BIT) > 0);

    // Calc normal
    nrm = select(
      (*tri).n1 * h.u + (*tri).n2 * h.v + (*tri).n0 * (1.0 - h.u - h.v), 
      (*tri).n0, // Simply use face normal in n0
      (((*tri).mtl >> 16) & FLAT) > 0);    
    nrm = normalize((vec4f(nrm, 0.0) * (*inst).transform).xyz);
  }

  // Get actual material data
  let data = materials[mtlId];

  if(any(data.color > vec3f(1.0))) {
    *emission = data.color;
    return false;
  }

  let inside = dot(r.dir, nrm) > 0;
  nrm = select(nrm, -nrm, inside);

  if(data.value > 1.0) {
    return evalMaterialDielectric(r, nrm, inside, data.color, data.value, attenuation, scatterDir);
  } else if(data.value > 0.0) {
    return evalMaterialMetal(r, nrm, data.color, data.value, attenuation, scatterDir);
  } else {
    return evalMaterialLambert(r, nrm, data.color, attenuation, scatterDir);
  }
}

fn render(initialRay: Ray) -> vec3f
{
  var ray = initialRay;
  var col = vec3f(1);
  var bounce = 0u;
  loop {
    var hit: Hit;
    hit.t = MAX_DISTANCE;
    intersectTlas(ray, &hit);
    if(hit.t < MAX_DISTANCE) {
      var att: vec3f;
      var emit: vec3f;
      var newDir: vec3f;
      if(evalMaterial(ray, hit, &att, &emit, &newDir)) {
        col *= att;
        ray = createRay(ray.ori + hit.t * ray.dir, newDir);
      } else {
        col *= emit;
        break;
      }
    } else {
      col *= globals.bgColor;
      break;
    }
    bounce += 1;
    if(bounce >= globals.maxBounces) {
      break;
    }
  }

  return col;
}

fn createPrimaryRay(pixelPos: vec2f) -> Ray
{
  var pixelSample = globals.pixelTopLeft + globals.pixelDeltaX * pixelPos.x + globals.pixelDeltaY * pixelPos.y;
  pixelSample += (rand() - 0.5) * globals.pixelDeltaX + (rand() - 0.5) * globals.pixelDeltaY;

  var eyeSample = globals.eye;
  if(globals.focAngle > 0) {
    let focRadius = globals.focDist * tan(0.5 * radians(globals.focAngle));
    let diskSample = rand2Disk();
    eyeSample += focRadius * (diskSample.x * globals.right + diskSample.y * globals.up);
  }

  return createRay(eyeSample, normalize(pixelSample - eyeSample));
}

@compute @workgroup_size(8,8)
fn computeMain(@builtin(global_invocation_id) globalId: vec3u)
{
  if(any(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  let index = globals.width * globalId.y + globalId.x;
  rngState = index ^ u32(globals.rngSeed * 0xffffffff); 

  var col = vec3f(0);
  for(var i=0u; i<globals.samplesPerPixel; i++) {
    col += render(createPrimaryRay(vec2f(globalId.xy)));
  }

  buffer[index] = vec4f(mix(buffer[index].xyz, col / f32(globals.samplesPerPixel), globals.weight), 1);
}

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  let pos = array<vec2f, 4>(vec2f(-1, 1), vec2f(-1), vec2f(1), vec2f(1, -1));
  return vec4f(pos[vertexIndex], 0, 1);
}

@fragment
fn fragmentMain(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  return vec4f(pow(buffer[globals.width * u32(pos.y) + u32(pos.x)].xyz, vec3f(0.4545)), 1);
}
