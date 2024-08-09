struct Camera
{
  eye:              vec3f,
  halfTanVertFov:   f32,
  right:            vec3f,
  focDist:          f32,
  up:               vec3f,
  halfTanFocAngle:  f32,
}

struct Config
{
  width:            u32,
  height:           u32,
  frame:            u32,
  samplesTaken:     u32,            // Bits 8-31 for samples taken, bits 0-7 max bounces
  pathCnt:          u32,
  extRayCnt:        u32,
  shadowRayCnt:     u32,
  pad0:             u32,
  gridDimPath:      vec4u,
  gridDimSRay:      vec4u,
  bgColor:          vec4f           // w is unused
}

struct PathState
{
  seed:             vec4u,          // Last rng seed used
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  pad0:             f32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, bounce num in bits 0-7
}

// General constants
const PI        = 3.141592;
const WG_SIZE   = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> camera: Camera;
@group(0) @binding(1) var<storage, read> config: Config;
@group(0) @binding(2) var<storage, read_write> pathStates: array<PathState>;

// State of rng seed
var<private> seed: vec4u;

// PCG 4D from Jarzynski/Olano: Hash Functions for GPU Rendering
fn rand4() -> vec4f
{
  seed = seed * 1664525u + 1013904223u;
  seed += seed.yzxy * seed.wxyz;
  seed = seed ^ (seed >> vec4u(16));
  seed += seed.yzxy * seed.wxyz;
  return ldexp(vec4f((seed >> vec4u(22)) ^ seed), vec4i(-32));
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

fn samplePixel(pixelPos: vec2f, r: vec2f) -> vec3f
{
  let height = 2.0 * camera.halfTanVertFov * camera.focDist;
  let width = height * f32(config.width) / f32(config.height);

  let right = width * camera.right;
  let down = -height * camera.up;

  let deltaX = right / f32(config.width);
  let deltaY = down / f32(config.height);

  let forward = cross(camera.right, camera.up);

  let topLeft = camera.eye - camera.focDist * forward - 0.5 * (right + down);
  let pixTopLeft = topLeft + 0.5 * (deltaX + deltaY);

  var pixelSample = pixTopLeft + deltaX * pixelPos.x + deltaY * pixelPos.y;
  pixelSample += (r.x - 0.5) * deltaX + (r.y - 0.5) * deltaY;

  return pixelSample;
}

fn sampleEye(r: vec2f) -> vec3f
{
  var eyeSample = camera.eye;
  if(camera.halfTanFocAngle > 0) {
    let focRadius = camera.focDist * camera.halfTanFocAngle;
    let diskSample = sampleDisk(r);
    eyeSample += focRadius * (diskSample.x * camera.right + diskSample.y * camera.up);
  }
  return eyeSample;
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = config.gridDimPath.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= config.pathCnt) {
    return;
  }

  // Pixel coordinates
  let w = config.width;
  let pix = vec2u(gidx % w, gidx / w);

  seed = vec4u(pix, config.frame, config.samplesTaken >> 8);

  let r0 = rand4();

  // Create new primary ray
  let ori = sampleEye(r0.xy);
  let dir = normalize(samplePixel(vec2f(pix), r0.zw) - ori);

  // Initialize new path
  pathStates[gidx].seed = seed;
  //pathStates[gidx].throughput = vec3f(1.0);
  pathStates[gidx].ori = ori;
  pathStates[gidx].dir = dir;
  pathStates[gidx].pidx = gidx << 8; // Bounce num is implicitly 0
}
