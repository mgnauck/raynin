/*struct Config
{
  frameData:        vec4u,          // x = bits 8-31 for width, bits 0-7 max bounces
                                    // y = bits 8-31 for height, bits 0-7 samples per pixel
                                    // z = current frame number
                                    // w = bits 8-31 for samples taken (before current frame), bits 0-7 frame's sample num
  pathStateGrid:    vec4u,          // w = path cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

const WG_SIZE = vec3u(16, 16, 1);

@group(0) @binding(0) var<storage, read_write> config: array<vec4u, 4>;

@compute @workgroup_size(1)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Calc grid dims for path state buf and set path cnt to ext ray cnt
  let extRayCnt = config[3].w;
  var dim = u32(ceil(sqrt(f32(extRayCnt)) / f32(WG_SIZE.x)));
  config[1] = vec4u(dim, dim, 1u, extRayCnt);

  // Calc grid dim for shadow ray buf and set shadow ray cnt
  let shadowRayCnt = config[2].w;
  dim = u32(ceil(sqrt(f32(shadowRayCnt)) / f32(WG_SIZE.x)));
  config[2] = vec4u(dim, dim, 1u, shadowRayCnt);

  // Reset ext ray cnt to 0
  config[3].w = 0u;
}

@compute @workgroup_size(1)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  if(globalId.x != 0) {
    return;
  }

  // Reset shadow ray cnt
  config[2].w = 0u;
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

  // Calc grid dims for path state buf and set path cnt to w * h (primary rays)
  config[1] = vec4u(u32(ceil(f32(w) / f32(WG_SIZE.x))), u32(ceil(f32(h) / f32(WG_SIZE.y))), 1u, w * h);

  // Reset shadow ray cnt and ext ray cnt
  config[2].w = 0u;
  config[3].w = 0u;
}
