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
  v0: vec3f,
  v1: vec3f,
  v2: vec3f,
  center: vec3f
}

struct TriData
{
  n0: vec3f,
  n1: vec3f,
  n2: vec3f,
  uv0: vec2f,
  uv1: vec2f,
  uv2: vec2f,
  pad: f32
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
  aabbMin: vec3f,
  triOfs: u32,
  aabbMax: vec3f,
  bvhNodeOfs: u32,
  transform: mat4x4f,
  invTransform: mat4x4f,
  id: u32,          // (mesh idx << 20) | (inst idx & 0xfffff)
  matId: u32        // (mat type << 24) | (mat idx & 0xffffff)
}

struct Mat
{
  albedo: vec3f,
  value: f32
}

struct Hit
{
  t: f32,
  u: f32,
  v: f32,
  obj: u32,         // (mesh idx << 20) | (inst idx & 0xfffff)
  tri: u32,
  nrm: vec3f
}

const EPSILON = 0.001;
const PI = 3.141592;
const MAX_DISTANCE = 3.402823466e+38;

const MAT_TYPE_LAMBERT = 1;
const MAT_TYPE_METAL = 2;
const MAT_TYPE_GLASS = 3;
const MAT_TYPE_EMITTER = 4;
const MAT_TYPE_ISOTROPIC = 5;

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<storage, read> tris: array<Tri>;
@group(0) @binding(2) var<storage, read> trisData: array<TriData>;
@group(0) @binding(3) var<storage, read> indices: array<u32>;
@group(0) @binding(4) var<storage, read> bvhNodes: array<BvhNode>;
@group(0) @binding(5) var<storage, read> tlasNodes: array<TlasNode>;
@group(0) @binding(6) var<storage, read> instances: array<Inst>;
@group(0) @binding(7) var<storage, read> materials: array<Mat>;
@group(0) @binding(8) var<storage, read_write> buffer: array<vec4f>;
//@group(0) @binding(9) var<storage, read_write> image: array<vec4f>;

var<private> bvhNodeStack: array<u32, 64>;
var<private> tlasNodeStack: array<u32, 64>;
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

// Moeller/Trumbore ray-triangle intersection
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/raytri/
fn intersectTri(ray: Ray, tri: Tri, objId: u32, triId: u32, h: ptr<function, Hit>)
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
    (*h).obj = objId;
    (*h).tri = triId;
  }
}

fn intersectBvh(ray: Ray, inst: Inst, h: ptr<function, Hit>)
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  loop {
    let node = &bvhNodes[inst.bvhNodeOfs + nodeIndex];
    let nodeStartIndex = (*node).startIndex;
    let nodeObjCount = (*node).objCount;
   
    if(nodeObjCount > 0) {
      // Leaf node, check all contained triangles
      for(var i=0u; i<nodeObjCount; i++) {
        let triId = indices[inst.triOfs + nodeStartIndex + i];
        intersectTri(ray, tris[triId], inst.id, triId, h);
      }
      if(nodeStackIndex > 0) {
        nodeStackIndex--;
        nodeIndex = bvhNodeStack[nodeStackIndex];
      } else {
        break;
      }
    } else {
      // Interior node, check aabbs of children
      let leftChildNode = &bvhNodes[inst.bvhNodeOfs + nodeStartIndex];
      let rightChildNode = &bvhNodes[inst.bvhNodeOfs + nodeStartIndex + 1];

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

fn intersectInst(ray: Ray, inst: Inst, h: ptr<function, Hit>)
{
  // Transform ray into object space of the instance
  var rayObjSpace: Ray;
  rayObjSpace.ori = (inst.invTransform * vec4f(ray.ori, 1.0)).xyz;
  rayObjSpace.dir = (inst.invTransform * vec4f(ray.dir, 0.0)).xyz;
  rayObjSpace.invDir = 1.0 / rayObjSpace.dir;

  intersectBvh(rayObjSpace, inst, h);
}

fn intersectTlas(ray: Ray, h: ptr<function, Hit>) -> bool
{
  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  loop {
    let node = &tlasNodes[nodeIndex];
    let nodeChildren = (*node).children;
   
    if(nodeChildren == 0) {
      // Leaf node with a single instance assigned 
      intersectInst(ray, instances[(*node).inst], h);
      if(nodeStackIndex > 0) {
        nodeStackIndex--;
        nodeIndex = tlasNodeStack[nodeStackIndex];
      } else {
        break;
      }
    } else {
      // Interior node, check aabbs of children
      let leftChildIndex = nodeChildren & 0xffff;
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

  if((*h).t < MAX_DISTANCE) {
    // TODO
    return true;
  }

  return false;
}

fn evalMaterialLambert(in: Ray, h: Hit, albedo: vec3f, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let nrm = select(h.nrm, -h.nrm, dot(in.dir, h.nrm) > 0);
  let dir = nrm + rand3UnitSphere();

  *scatterDir = select(normalize(dir), nrm, all(abs(dir) < vec3f(EPSILON)));
  *attenuation = albedo;
  return true;
}

fn evalMaterialMetal(in: Ray, h: Hit, albedo: vec3f, fuzzRadius: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let nrm = select(h.nrm, -h.nrm, dot(in.dir, h.nrm) > 0);
  let dir = reflect(in.dir, nrm);

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

fn evalMaterialGlass(in: Ray, h: Hit, albedo: vec3f, refractionIndex: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let inside = dot(in.dir, h.nrm) > 0;
  let nrm = select(h.nrm, -h.nrm, inside);
  let refracIndexRatio = select(1 / refractionIndex, refractionIndex, inside);
  
  let cosTheta = min(dot(-in.dir, nrm), 1);
  //let sinTheta = sqrt(1 - cosTheta * cosTheta);
  //
  //var dir: vec3f;
  //if(refracIndexRatio * sinTheta > 1 || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
  //  dir = reflect(in.dir, nrm);
  //} else {
  //  dir = refract(in.dir, nrm, refracIndexRatio);
  //}

  var dir = refract(in.dir, nrm, refracIndexRatio);
  if(all(dir == vec3f(0)) || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
    dir = reflect(in.dir, nrm);
  }

  *scatterDir = dir;
  *attenuation = albedo;
  return true;
}

fn evalMaterialIsotropic(in: Ray, h: Hit, albedo: vec3f, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  *scatterDir = rand3UnitSphere();
  *attenuation = albedo;
  return true;
}

/*fn evalMaterial(in: Ray, h: Hit, attenuation: ptr<function, vec3f>, emission: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let data = materials[h.matOfs];
  switch(h.matType)
  {
    case MAT_TYPE_LAMBERT: {
      return evalMaterialLambert(in, h, data.xyz, attenuation, scatterDir);
    }
    case MAT_TYPE_METAL: {
      return evalMaterialMetal(in, h, data.xyz, data.w, attenuation, scatterDir);
    }
    case MAT_TYPE_GLASS: {
      return evalMaterialGlass(in, h, data.xyz, data.w, attenuation, scatterDir);
    }
    case MAT_TYPE_ISOTROPIC: {
      return evalMaterialIsotropic(in, h, data.xyz, attenuation, scatterDir);
    }
    case MAT_TYPE_EMITTER: {
      *emission = data.xyz;
      return false;
    }
    default: {
      // Error material
      *emission = vec3f(99999, 0, 0);
      return false;
    }
  }
}*/

fn render(initialRay: Ray) -> vec3f
{
  var ray = initialRay;
  var col = vec3f(1);
  var bounce = 0u;
  loop {
    var hit: Hit;
    hit.t = MAX_DISTANCE;
    if(intersectTlas(ray, &hit)) {
      col = vec3f(1, 0, 0);
      break;
      /*var att: vec3f;
      var emit: vec3f;
      var newDir: vec3f;
      if(evalMaterial(ray, hit, &att, &emit, &newDir)) {
        col *= att;
        ray = createRay(hit.pos, newDir);
      } else {
        col *= emit;
        break;
      }*/
    } else {
      col *= globals.bgColor;
      break;
    }
    /*bounce += 1;
    if(bounce >= globals.maxBounces) {
      break;
    }*/
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
  if(all(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  let index = globals.width * globalId.y + globalId.x;
  rngState = index ^ u32(globals.rngSeed * 0xffffffff); 

  var col = vec3f(0);
  for(var i=0u; i<globals.samplesPerPixel; i++) {
    col += render(createPrimaryRay(vec2f(globalId.xy)));
  }

  let outCol = mix(buffer[index].xyz, col / f32(globals.samplesPerPixel), globals.weight);

  buffer[index] = vec4f(outCol, 1);
  //image[index] = vec4f(pow(outCol, vec3f(0.4545)), 1);
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
  //return image[globals.width * u32(pos.y) + u32(pos.x)];
}
