/*struct Config
{
  frameData:        vec4u,          // x = width
                                    // y = bits 8-31 for height, bits 4-7 unused, bits 0-3 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

@group(0) @binding(0) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(1) var<storage, read> colBuf: array<vec4f>;
@group(0) @binding(2) var<storage, read> accumColBuf: array<vec4f>;

@vertex
fn vm(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  /*
  // Workaround for below code which does work with with Naga
  switch(vertexIndex)
  {
    case 0u: {
      return vec4f(vec2f(-1.0, 1.0), 0.0, 1.0);
    }
    case 1u: {
      return vec4f(vec2f(-1.0), 0.0, 1.0);
    }
    case 2u: {
      return vec4f(vec2f(1.0), 0.0, 1.0);
    }
    default: {
      return vec4f(vec2f(1.0, -1.0), 0.0, 1.0);
    }
  }
  */

  let pos = array<vec2f, 4>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0));
  return vec4f(pos[vertexIndex], 0.0, 1.0);
}

fn octToDir(oct: vec2f) -> vec3f
{
  let v = vec3f(oct, 1.0 - abs(oct.x) - abs(oct.y));
  return normalize(select(v, vec3f((1.0 - abs(v.yx)) * (step(vec2f(0.0), v.xy) * 2.0 - vec2f(1.0)), v.z), v.z < 0.0));
}

// Accumulated direct and indirect illumination
@fragment
fn m(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  let gidx = w * u32(pos.y) + u32(pos.x);
  let dcol = accumColBuf[        gidx].xyz;
  let icol = accumColBuf[w * h + gidx].xyz;
  return vec4f(pow(dcol + icol, vec3f(0.4545)), 1.0);
}

// No reprojection/no filter, just pass through from direct/indirect illumination buffer
@fragment
fn m1(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  let gidx = w * u32(pos.y) + u32(pos.x);
  let dcol = colBuf[        gidx].xyz;
  let icol = colBuf[w * h + gidx].xyz;
  return vec4f(pow((dcol + icol) / f32(frame.w), vec3f(0.4545)), 1.0);
}
