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
  tmin: f32,
  t: f32
}

struct BvhNode
{
  aabbMin: vec3f,
  startIndex: u32, // Either index of first object or left child node
  aabbMax: vec3f,
  objCount: u32
}

struct Object
{
  shapeType: u32,
  shapeOfs: u32,
  matType: u32,
  matOfs: u32,
}

struct Hit
{
  pos: vec3f,
  nrm: vec3f,
  matType: u32,
  matOfs: u32
}

const EPSILON = 0.001;
const PI = 3.141592;
const MAX_DISTANCE = 3.402823466e+38;

const SHAPE_TYPE_SPHERE = 1u;
const SHAPE_TYPE_BOX = 2u;
const SHAPE_TYPE_CYLINDER = 3u;
const SHAPE_TYPE_QUAD = 4u;
const SHAPE_TYPE_MESH = 5u;

const MAT_TYPE_LAMBERT = 1u;
const MAT_TYPE_METAL = 2u;
const MAT_TYPE_GLASS = 3u;
const MAT_TYPE_EMITTER = 4u;
const MAT_TYPE_ISOTROPIC = 5u;

const vertexPos = array<vec2f, 4u>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0));

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<storage, read> bvhNodes: array<BvhNode>;
@group(0) @binding(2) var<storage, read> indices: array<u32>;
@group(0) @binding(3) var<storage, read> objects: array<Object>;
@group(0) @binding(4) var<storage, read> shapes: array<vec4f>;
@group(0) @binding(5) var<storage, read> materials: array<vec4f>;
@group(0) @binding(6) var<storage, read_write> buffer: array<vec4f>;
@group(0) @binding(7) var<storage, read_write> image: array<vec4f>;

var<private> nodeStack: array<u32, 32>; // Fixed size
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
  let u = 2.0 * rand() - 1.0;
  let theta = 2.0 * PI * rand();
  let r = sqrt(1.0 - u * u);
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
  let theta = 2.0 * PI * rand();
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

fn createRay(ori: vec3f, dir: vec3f, tmin: f32, tmax: f32) -> Ray
{
  var r: Ray;
  r.ori = ori;
  r.dir = dir;
  r.tmin = tmin;
  r.t = tmax;
  r.invDir = 1.0 / r.dir;
  return r;
}

fn intersectAabb(ray: Ray, minExt: vec3f, maxExt: vec3f) -> f32
{
  let t0 = (minExt - ray.ori) * ray.invDir;
  let t1 = (maxExt - ray.ori) * ray.invDir;

  let tmin = maxComp(min(t0, t1));
  let tmax = minComp(max(t0, t1));
  
  return select(MAX_DISTANCE, tmin, tmin <= tmax && tmin < ray.t && tmax > ray.tmin);
}

fn intersectSphere(ray: Ray, center: vec3f, radius: f32) -> f32
{
  let oc = ray.ori - center;
  let a = dot(ray.dir, ray.dir);
  let b = dot(oc, ray.dir); // half
  let c = dot(oc, oc) - radius * radius;

  let d = b * b - a * c;
  if(d < 0.0) {
    return MAX_DISTANCE;
  }

  let sqrtd = sqrt(d);
  var t = (-b - sqrtd) / a;
  if(t <= ray.tmin || ray.t <= t) {
    t = (-b + sqrtd) / a;
    if(t <= ray.tmin || ray.t <= t) {
      return MAX_DISTANCE;
    }
  }

  return t;
}

fn completeHitSphere(ray: Ray, center: vec3f, radius: f32, h: ptr<function, Hit>)
{
  (*h).pos = ray.ori + ray.t * ray.dir;
  (*h).nrm = ((*h).pos - center) / radius;
}

fn intersectQuad(ray: Ray, q: vec3f, u: vec3f, v: vec3f) -> f32
{
  let n = cross(u, v);
  let nrm = normalize(n);
  let denom = dot(nrm, ray.dir);

  if(abs(denom) < EPSILON) {
    return MAX_DISTANCE;
  }

  let t = (dot(nrm, q) - dot(nrm, ray.ori)) / denom;
  if(t < ray.tmin || t > ray.t) {
    return MAX_DISTANCE;
  }

  let w = n / dot(n, n);
  let pos = ray.ori + t * ray.dir;
  let planar = pos - q;
  let a = dot(w, cross(planar, v));
  let b = dot(w, cross(u, planar));

  return select(t, MAX_DISTANCE, a < 0.0 || 1.0 < a || b < 0.0 || 1.0 < b);
}

fn completeHitQuad(ray: Ray, q: vec3f, u: vec3f, v: vec3f, h: ptr<function, Hit>)
{
  (*h).pos = ray.ori + ray.t * ray.dir;
  (*h).nrm = normalize(cross(u, v));
}

/*fn intersectConstantMedium(ray: Ray, negInvDensity: f32, refIndex: u32) -> f32
{
  var newRay = ray;
  newRay.tmin = -MAX_DISTANCE;
  newRay.t = MAX_DISTANCE;
  var t1 = intersectObjectNonRecursive(newRay, refIndex);
  if(t1 >= MAX_DISTANCE) {
    return MAX_DISTANCE;
  }

  newRay.tmin = t1 + EPSILON;
  var t2 = intersectObjectNonRecursive(newRay, refIndex);
  if(t2 >= MAX_DISTANCE) {
    return MAX_DISTANCE;
  }

  if(t1 < ray.tmin) {
    t1 = ray.tmin;
  }

  if(t2 > ray.t) {
    t2 = ray.t;
  }

  if(t1 >= t2) {
    return MAX_DISTANCE;
  }

  if(t1 < 0.0) {
    t1 = 0.0;
  }

  let rayLen = length(ray.dir);
  let dist = negInvDensity * log(rand());
  if(dist > (t2 - t1) * rayLen) {
    return MAX_DISTANCE;
  }

  return t1 + dist / rayLen;
}

fn completeHitConstantMedium(ray: Ray, h: ptr<function, Hit>)
{
  (*h).pos = ray.ori + ray.t * ray.dir;
}*/

fn evalMaterialLambert(in: Ray, h: Hit, albedo: vec3f, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let nrm = select(h.nrm, -h.nrm, dot(in.dir, h.nrm) > 0.0);
  let dir = nrm + rand3UnitSphere();

  *scatterDir = select(normalize(dir), nrm, all(abs(dir) < vec3f(EPSILON)));
  *attenuation = albedo;
  return true;
}

fn evalMaterialMetal(in: Ray, h: Hit, albedo: vec3f, fuzzRadius: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let nrm = select(h.nrm, -h.nrm, dot(in.dir, h.nrm) > 0.0);
  let dir = reflect(in.dir, nrm);

  *scatterDir = normalize(dir + fuzzRadius * rand3UnitSphere());
  *attenuation = albedo;
  return dot(*scatterDir, nrm) > 0.0;
}

fn schlickReflectance(cosTheta: f32, refractionIndexRatio: f32) -> f32
{
  var r0 = (1.0 - refractionIndexRatio) / (1.0 + refractionIndexRatio);
  r0 = r0 * r0;
  return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

fn evalMaterialGlass(in: Ray, h: Hit, albedo: vec3f, refractionIndex: f32, attenuation: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
{
  let inside = dot(in.dir, h.nrm) > 0.0;
  let nrm = select(h.nrm, -h.nrm, inside);
  let refracIndexRatio = select(1.0 / refractionIndex, refractionIndex, inside);
  
  let cosTheta = min(dot(-in.dir, nrm), 1.0);
  /*let sinTheta = sqrt(1 - cosTheta * cosTheta);

  var dir: vec3f;
  if(refracIndexRatio * sinTheta > 1.0 || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
    dir = reflect(in.dir, nrm);
  } else {
    dir = refract(in.dir, nrm, refracIndexRatio);
  }*/

  var dir = refract(in.dir, nrm, refracIndexRatio);
  if(all(dir == vec3f(0.0)) || schlickReflectance(cosTheta, refracIndexRatio) > rand()) {
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

fn evalMaterial(in: Ray, h: Hit, attenuation: ptr<function, vec3f>, emission: ptr<function, vec3f>, scatterDir: ptr<function, vec3f>) -> bool
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
      *emission = vec3f(99999.0, 0.0, 0.0);
      return false;
    }
  }
}

fn intersectObject(ray: Ray, objIndex: u32) -> f32
{
  let obj = &objects[indices[objIndex]];
  let data = shapes[(*obj).shapeOfs];
  
  switch((*obj).shapeType) {
    case SHAPE_TYPE_SPHERE: {
      return intersectSphere(ray, data.xyz, data.w);
    }
    case SHAPE_TYPE_QUAD: {
      let u = shapes[(*obj).shapeOfs + 1u];
      let v = shapes[(*obj).shapeOfs + 2u];
      return intersectQuad(ray, data.xyz, u.xyz, v.xyz);
    }
    default: {
      return ray.t;
    }
  }
}

fn intersectObjects(ray: ptr<function, Ray>, objStartIndex: u32, objCount: u32, objId: ptr<function, u32>)
{
  for(var i=objStartIndex; i<objStartIndex + objCount; i++) {
    let currDist = intersectObject(*ray, i);
    if(currDist < (*ray).t) {
      (*ray).t = currDist;
      *objId = i;
    }
  }
}

fn intersectScene(ray: ptr<function, Ray>, hit: ptr<function, Hit>) -> bool
{
  var objId: u32;
  
  //intersectObjects(ray, 0, arrayLength(&objects), &objId);

  var nodeIndex = 0u;
  var nodeStackIndex = 0u;

  loop {
    let node = &bvhNodes[nodeIndex];
    let nodeStartIndex = (*node).startIndex;
    let nodeObjCount = (*node).objCount;
   
    if(nodeObjCount > 0u) {
      intersectObjects(ray, nodeStartIndex, nodeObjCount, &objId);
      if(nodeStackIndex > 0u) {
        nodeStackIndex--;
        nodeIndex = nodeStack[nodeStackIndex];
      } else {
        break;
      }
    } else {
      let leftChildNode = &bvhNodes[nodeStartIndex];
      let rightChildNode = &bvhNodes[nodeStartIndex + 1u];

      let leftDist = intersectAabb(*ray, (*leftChildNode).aabbMin, (*leftChildNode).aabbMax);
      let rightDist = intersectAabb(*ray, (*rightChildNode).aabbMin, (*rightChildNode).aabbMax);
 
      let switchNodes = leftDist > rightDist;
      let nearNodeDist = select(leftDist, rightDist, switchNodes);
      let farNodeDist = select(rightDist, leftDist, switchNodes);
      let nearNodeIndex = select(nodeStartIndex, nodeStartIndex + 1u, switchNodes);
      let farNodeIndex = select(nodeStartIndex + 1u, nodeStartIndex, switchNodes);

      if(nearNodeDist < MAX_DISTANCE) {
        nodeIndex = nearNodeIndex;
        if(farNodeDist < MAX_DISTANCE) {
          nodeStack[nodeStackIndex] = farNodeIndex;
          nodeStackIndex++;
        }
      } else {
        if(nodeStackIndex > 0u) {
          nodeStackIndex--;
          nodeIndex = nodeStack[nodeStackIndex];
        } else {
          break;
        }
      }
    }
  }

  if((*ray).t < MAX_DISTANCE) {
    let obj = &objects[indices[objId]];
    let data = shapes[(*obj).shapeOfs];
    switch((*obj).shapeType) {
      case SHAPE_TYPE_SPHERE: {
        completeHitSphere(*ray, data.xyz, data.w, hit);
      }
      case SHAPE_TYPE_QUAD: {
        let u = shapes[(*obj).shapeOfs + 1u];
        let v = shapes[(*obj).shapeOfs + 2u];
        completeHitQuad(*ray, data.xyz, u.xyz, v.xyz, hit);
      }
      case SHAPE_TYPE_MESH: {
        return false;
      }
      default: {
        return false;
      }
    }
    (*hit).matType = (*obj).matType;
    (*hit).matOfs = (*obj).matOfs;
    return true;
  }

  return false;
}

fn render(initialRay: Ray) -> vec3f
{
  var ray = initialRay;
  var col = vec3f(1.0);
  var bounce = 0u;
  loop {
    var hit: Hit;
    if(intersectScene(&ray, &hit)) {
      var att: vec3f;
      var emit: vec3f;
      var newDir: vec3f;
      if(evalMaterial(ray, hit, &att, &emit, &newDir)) {
        col *= att;
        ray = createRay(hit.pos, newDir, EPSILON, MAX_DISTANCE);
      } else {
        col *= emit;
        break;
      }
    } else {
      col *= globals.bgColor;
      break;
    }
    bounce += 1u;
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
  if(globals.focAngle > 0.0) {
    let focRadius = globals.focDist * tan(0.5 * radians(globals.focAngle));
    let diskSample = rand2Disk();
    eyeSample += focRadius * (diskSample.x * globals.right + diskSample.y * globals.up);
  }

  return createRay(eyeSample, normalize(pixelSample - eyeSample), EPSILON, MAX_DISTANCE);
}

@compute @workgroup_size(8,8)
fn computeMain(@builtin(global_invocation_id) globalId: vec3u)
{
  if(all(globalId.xy >= vec2u(globals.width, globals.height))) {
    return;
  }

  let index = globals.width * globalId.y + globalId.x;
  rngState = index ^ u32(globals.rngSeed * 0xffffffff); 

  var col = vec3f(0.0);
  for(var i=0u; i<globals.samplesPerPixel; i++) {
    col += render(createPrimaryRay(vec2f(globalId.xy)));
  }

  let outCol = mix(buffer[index].xyz, col / f32(globals.samplesPerPixel), globals.weight);

  buffer[index] = vec4f(outCol, 1.0);
  image[index] = vec4f(pow(outCol, vec3f(0.4545)), 1.0);
}

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  return vec4f(vertexPos[vertexIndex], 0.0, 1.0);
 
  // Firefox/Naga workaround for above array access
  /*switch(vertexIndex) {
    case 0u: {
      return vec4f(-1.0, 1.0, 0.0, 1.0);
    }
    case 1u: {
      return vec4f(-1.0, -1.0, 0.0, 1.0);
    } 
    case 2u: {
      return vec4f(1.0, 1.0, 0.0, 1.0);
    }
    default: {
      return vec4f(1.0, -1.0, 0.0, 1.0);
    }
  }*/
}

@fragment
fn fragmentMain(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  return image[globals.width * u32(pos.y) + u32(pos.x)];
}
