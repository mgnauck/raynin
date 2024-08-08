struct Frame
{
  width:        u32,
  height:       u32,
  frame:        u32,
  samplesTaken: u32,    // Bits 8-31 for samples taken, bits 0-7 max bounces
  pathCnt:      u32,
  extRayCnt:    u32,
  shadowRayCnt: u32,
  pad0:         u32,
  gridDimPath:  vec4u,
  gridDimSRay:  vec4u
}

const WG_SIZE = vec3u(8, 8, 1);

@group(0) @binding(0) var<storage, read_write> frame: Frame;

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
  var dim = u32(ceil(sqrt(f32(pathCnt)) / f32(WG_SIZE.x)));
  frame.gridDimPath = vec4u(dim, dim, 1u, 0u);

  dim = u32(ceil(sqrt(f32(srayCnt)) / f32(WG_SIZE.x)));
  frame.gridDimSRay = vec4u(dim, dim, 1u, 0u);
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
  frame.samplesTaken += 1u << 8;

  let w = frame.width;
  let h = frame.height;

  // Init counters for primary ray generation
  frame.pathCnt = w * h;
  frame.extRayCnt = 0u;
  frame.shadowRayCnt = 0u;

  // Calculate workgroup grid size to dispatch indirectly
  frame.gridDimPath = vec4u(u32(ceil(f32(w) / f32(WG_SIZE.x))), u32(ceil(f32(h) / f32(WG_SIZE.y))), 1u, 0u);
}
