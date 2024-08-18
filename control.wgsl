/*struct Config
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
  bgColor:          vec4f   // w is unused
}*/

const WG_SIZE = vec3u(16, 16, 1);

@group(0) @binding(0) var<storage, read_write> config: array<vec4u, 5>;

@compute @workgroup_size(1)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  let counter = config[1];

  // Reset counters
  config[1] = vec4u(counter.y, 0u, counter.z, 0u);

  // Calculate workgroup grid sizes for indirect dispatch
  var dim = u32(ceil(sqrt(f32(counter.y)) / f32(WG_SIZE.x))); // y = extRayCnt
  config[2] = vec4u(dim, dim, 1u, 0u); // gridDimPath

  dim = u32(ceil(sqrt(f32(counter.z)) / f32(WG_SIZE.x))); // z = shadowRayCnt
  config[3] = vec4u(dim, dim, 1u, 0u); // gridDimSRay
}

@compute @workgroup_size(1)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Reset shadow ray cnt
  config[1].z = 0u;
}

@compute @workgroup_size(1)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  let frame = config[0];
  let w = frame.x >> 8;
  let h = frame.y >> 8;

  // Update sample num of current frame
  config[0].w += 1u;

  // Init counters for primary ray generation
  config[1] = vec4u(w * h, 0u, 0u, 0u);

  // Calculate workgroup grid size to dispatch indirectly (= gridDimWidth)
  config[2] = vec4u(u32(ceil(f32(w) / f32(WG_SIZE.x))), u32(ceil(f32(h) / f32(WG_SIZE.y))), 1u, 0u);
}
