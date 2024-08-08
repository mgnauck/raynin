struct Frame
{
  width:        u32,
  height:       u32,
  spp:          u32,
  frame:        u32,
  bouncesSpp:   u32,            // Bits 8-31 for samples taken, bits 0-7 max bounces
  pathCnt:      u32,
  extRayCnt:    u32,
  shadowRayCnt: u32,
  gridDimPath:  vec4u,
  gridDimSRay:  vec4u
}

@group(0) @binding(0) var<storage, read_write> frame: Frame;

const size = 8.0;
const dim = 128.0;

@compute @workgroup_size(1)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  let pathCnt = frame.extRayCnt;
  let srayCnt = frame.shadowRayCnt;

  // Reset counters
  frame.pathCnt = pathCnt;
  frame.extRayCnt = 0u;

  // Calculate workgroup grid sizes for indirect dispatch
  //var dim = u32(ceil(sqrt(f32(pathCnt)) / 8.0));
  //frame.gridDimPath = vec4u(dim, dim, 1u, 0u);
  //dim = u32(ceil(sqrt(f32(srayCnt)) / 8.0));
  //frame.gridDimSRay = vec4u(dim, dim, 1u, 0u);
  
  frame.gridDimPath = vec4u(u32(dim), u32(ceil(f32(pathCnt) / (dim * size))), 1u, 0u);
  frame.gridDimSRay = vec4u(u32(dim), u32(ceil(f32(srayCnt) / (dim * size))), 1u, 0u);
}

@compute @workgroup_size(1)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  frame.shadowRayCnt = 0u;
}

@compute @workgroup_size(1)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Update samples taken in bits 8-31
  frame.bouncesSpp += 1u << 8;

  let w = frame.width;
  let h = frame.height;

  // Init counters for primary ray generation
  frame.pathCnt = w * h;
  frame.extRayCnt = 0u;
  frame.shadowRayCnt = 0u;

  // Calculate workgroup grid size to dispatch indirectly
  //frame.gridDimPath = vec4u(u32(ceil(f32(w) / 8.0)), u32(ceil(f32(h) / 8.0)), 1u, 0u);

  frame.gridDimPath = vec4u(u32(dim), u32(ceil(f32(w * h) / (dim * size))), 1u, 0u);
}
