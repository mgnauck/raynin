struct Global
{
  // Config
  bgColor:      vec3f,
  tlasNodeOfs:  u32,
  // Camera
  eye:          vec3f,
  vertFov:      f32,
  right:        vec3f,
  focDist:      f32,
  up:           vec3f,
  focAngle:     f32,
  // View
  pixelDeltaX:  vec3f,
  pad1:         f32,
  pixelDeltaY:  vec3f,
  pad2:         f32,
  pixelTopLeft: vec3f,
  pad3:         f32
}

struct Frame
{
  width:        u32,
  height:       u32,
  frame:        u32,
  bouncesSpp:   u32             // Bits 8-31 for gathered spp, bits 0-7 max bounces 
}

struct Ray
{
  ori:          vec3f,
  pad0:         f32,
  dir:          vec3f,
  pad1:         f32
}

struct PathData
{
  seed:         vec4u,          // Last rng seed used
  throughput:   vec3f,
  pdf:          f32,
  ori:          vec3f,
  pad0:         f32,
  dir:          vec3f,
  pidx:         u32             // Pixel idx in bits 8-31, bit 4-7 TBD, bounce num in bits 0-3
}

struct Hit
{
  t:            f32,
  u:            f32,
  v:            f32,
  e:            u32             // (tri id << 16) | (inst id & 0xffff)
}

struct State
{
  rayCnt:       array<u32, 2u>,
  shadowRayCnt: u32,
  cntIdx:       u32,            // Index into current buf/counter (currently bit 0 only).
  hits:         array<Hit>      // TODO This is here because we have a limit of 8 storage buffers :(
}

// General constants
const PI                  = 3.141592;

@group(0) @binding(0) var<uniform> globals: Global;
@group(0) @binding(1) var<uniform> frame: Frame;
@group(0) @binding(2) var<storage, read_write> rays: array<Ray>;
@group(0) @binding(3) var<storage, read_write> pathData: array<PathData>;
@group(0) @binding(4) var<storage, read> state: State;

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
  var pixelSample = globals.pixelTopLeft + globals.pixelDeltaX * pixelPos.x + globals.pixelDeltaY * pixelPos.y;
  pixelSample += (r.x - 0.5) * globals.pixelDeltaX + (r.y - 0.5) * globals.pixelDeltaY;
  return pixelSample;
}

fn sampleEye(r: vec2f) -> vec3f
{
  var eyeSample = globals.eye;
  if(globals.focAngle > 0) {
    let focRadius = globals.focDist * tan(0.5 * radians(globals.focAngle));
    let diskSample = sampleDisk(r);
    eyeSample += focRadius * (diskSample.x * globals.right + diskSample.y * globals.up);
  }
  return eyeSample;
}

@compute @workgroup_size(8, 8)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  if(any(globalId.xy >= vec2u(frame.width, frame.height))) {
    return;
  }

  seed = vec4u(globalId.xy, frame.frame, frame.bouncesSpp >> 8);

  let r0 = rand4();

  let gidx = frame.width * globalId.y + globalId.x;
  let bidx = (frame.width * frame.height * (state.cntIdx & 0x1)) + gidx;

  // Create primary ray
  let ori = sampleEye(r0.xy);
  let dir = normalize(samplePixel(vec2f(globalId.xy), r0.zw) - ori);
  rays[gidx].ori = ori;
  rays[gidx].dir = dir;

  // Initialize new path data
  pathData[bidx].seed = seed;
  pathData[bidx].throughput = vec3f(1.0);
  pathData[bidx].ori = ori;
  pathData[bidx].dir = dir;
  pathData[bidx].pidx = gidx << 8; // Bounce num is implicitly 0 (bits 0-3)
}
