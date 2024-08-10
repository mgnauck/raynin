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
  bgColor:          vec4f   // w is unused
}

const WG_SIZE = vec3u(16, 16, 1);

@group(0) @binding(0) var<storage, read_write> config: Config;

@compute @workgroup_size(1)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  let pathCnt = config.extRayCnt;
  let srayCnt = config.shadowRayCnt;

  // Reset counters
  config.pathCnt = pathCnt;
  config.extRayCnt = 0u;

  // Calculate workgroup grid sizes for indirect dispatch
  var dim = u32(ceil(sqrt(f32(pathCnt)) / f32(WG_SIZE.x)));
  config.gridDimPath = vec4u(dim, dim, 1u, 0u);

  dim = u32(ceil(sqrt(f32(srayCnt)) / f32(WG_SIZE.x)));
  config.gridDimSRay = vec4u(dim, dim, 1u, 0u);
}

@compute @workgroup_size(1)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  config.shadowRayCnt = 0u;
}

@compute @workgroup_size(1)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Update sample num of current frame
  config.samplesTaken += 1u;

  let w = config.width >> 8;
  let h = config.height >> 8;

  // Init counters for primary ray generation
  config.pathCnt = w * h;
  config.extRayCnt = 0u;
  config.shadowRayCnt = 0u;

  // Calculate workgroup grid size to dispatch indirectly
  config.gridDimPath = vec4u(u32(ceil(f32(w) / f32(WG_SIZE.x))), u32(ceil(f32(h) / f32(WG_SIZE.y))), 1u, 0u);
}
