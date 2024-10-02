/*struct Camera
{
  eye:              vec3f,
  halfTanVertFov:   f32,
  right:            vec3f,
  focDist:          f32,
  up:               vec3f,
  halfTanFocAngle:  f32,
}*/

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

/*struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, flags in bits 4-7, bounce num in bits 0-3
}*/

// General constants
const PI        = 3.141592;
const WG_SIZE   = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> camera: array<vec4f, 3>;
@group(0) @binding(1) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(2) var<storage, read_write> pathStates: array<vec4f>;

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

fn rand2() -> vec2f
{
  return vec2f(rand(), rand());
}

/*fn hammersley(i: u32, n: u32) -> vec2f
{
  return vec2f(f32(i) / f32(n), f32(reverseBits(i)) * 2.3283064365386963e-10);
}

fn rand2(i: u32, n: u32) -> vec2f
{
  return hammersley((seed + i) % n, n);
}*/

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

fn samplePixel(pixelPos: vec2f, eye: vec4f, right: vec4f, up: vec4f, res: vec2f, r0: vec2f) -> vec3f
{
  let vheight = 2.0 * eye.w * right.w; // 2 * halfTanVertFov * focDist
  let vwidth = vheight * res.x / res.y;

  let vright = vwidth * right.xyz;
  let vdown = -vheight * up.xyz;

  let deltaX = vright / res.x;
  let deltaY = vdown / res.y;

  let forward = cross(right.xyz, up.xyz);

  let topLeft = eye.xyz - right.w * forward - 0.5 * (vright + vdown); // right.w = focDist
  let pixTopLeft = topLeft + 0.5 * (deltaX + deltaY);

  var pixelSample = pixTopLeft + deltaX * pixelPos.x + deltaY * pixelPos.y;
  pixelSample += (r0.x - 0.5) * deltaX + (r0.y - 0.5) * deltaY;

  return pixelSample;
}

fn sampleEye(eye: vec4f, right: vec4f, up: vec4f, r0: vec2f) -> vec3f
{
  // Jitter eye pos for DOF
  let focRadius = right.w * up.w; // focDist * halfTanFocAngle
  let diskSample = sampleDisk(r0);
  return eye.xyz + focRadius * (diskSample.x * right.xyz + diskSample.y * up.xyz);
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x >> 16;
  let h = frame.y >> 16;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let gidx = w * globalId.y + globalId.x;

  // Set seed based on pixel index, current frame (z) and sample num (w)
  seed = wangHash(gidx * 32467 + frame.z * 23 + frame.w * 6173);

  // Read camera values
  let e = camera[0];
  let r = camera[1];
  let u = camera[2];

  let r0 = rand2();
  let r1 = rand2();

  // Create new primary ray
  //let ori = e.xyz;
  let ori = sampleEye(e, r, u, r0);
  let dir = normalize(samplePixel(vec2f(globalId.xy), e, r, u, vec2f(f32(w), f32(h)), r1) - ori);

  // Initialize new path
  // Do not initialize throughput/pdf, will do in shade.wgsl for primary ray
  let ofs = w * h;
  pathStates[       ofs + gidx] = vec4f(ori, bitcast<f32>(seed));
  pathStates[(ofs << 1) + gidx] = vec4f(dir, bitcast<f32>((gidx << 8))); // Flags and bounce num are implicitly 0
}
