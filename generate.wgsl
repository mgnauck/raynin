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

struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
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
var<private> seed: u32;

fn wangHash(x: u32) -> u32
{
  var v = (x ^ 61) ^ (x >> 16);
  v *= 9u;
  v ^= v >> 4u;
  v *= 0x27d4eb2du;
  v ^= v >> 15u;
  return v;
}

fn rand() -> f32
{
  // xorshift32
  seed ^= seed << 13u;
  seed ^= seed >> 17u;
  seed ^= seed << 5u;
  return bitcast<f32>(0x3f800000 | (seed >> 9)) - 1.0;
}

fn rand4() -> vec4f
{
  return vec4f(rand(), rand(), rand(), rand());
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

fn samplePixel(pixelPos: vec2f, r: vec2f, res: vec2f) -> vec3f
{
  let height = 2.0 * camera.halfTanVertFov * camera.focDist;
  let width = height * res.x / res.y;

  let right = width * camera.right;
  let down = -height * camera.up;

  let deltaX = right / res.x;
  let deltaY = down / res.y;

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
  let w = config.width >> 8;
  let h = config.height >> 8;

  let gidx = w * globalId.y + globalId.x;
  if(gidx >= w * h) {
    return;
  }

  // Set seed based on pixel index, current frame and sample num of the frame
  seed = wangHash(gidx * 32467 + config.frame * 23 + (config.samplesTaken & 0xff) * 6173);

  let r = rand4();

  // Create new primary ray
  let ori = sampleEye(r.xy);
  let dir = normalize(samplePixel(vec2f(globalId.xy), r.zw, vec2f(f32(w), f32(h))) - ori);

  // Initialize new path
  pathStates[gidx].seed = seed;
  //pathStates[gidx].throughput = vec3f(1.0);
  pathStates[gidx].ori = ori;
  pathStates[gidx].dir = dir;
  pathStates[gidx].pidx = gidx << 8; // Bounce num is implicitly 0
}
