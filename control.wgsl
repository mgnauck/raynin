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
}

@group(0) @binding(0) var<storage, read_write> frame: Frame;

@compute @workgroup_size(1)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Update counters
  frame.pathCnt = frame.width * frame.height /* * frame.spp */;
  frame.extRayCnt = 0;
  frame.shadowRayCnt = 0;
}

@compute @workgroup_size(1)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Update counters
  frame.pathCnt = frame.extRayCnt;
  frame.extRayCnt = 0;
  frame.shadowRayCnt = 0;
}

@compute @workgroup_size(1)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Update samples taken
  frame.bouncesSpp = (((frame.bouncesSpp >> 8) + 1) << 8) | (frame.bouncesSpp & 0xff);
}
