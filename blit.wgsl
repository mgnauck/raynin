struct Frame
{
  width:        u32,
  height:       u32,
  frame:        u32,
  bouncesSpp:   u32             // Bits 8-31 for gathered spp, bits 0-7 max bounces 
}

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<storage, read> accum: array<vec4f>;

@vertex
fn quad(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  // Workaround for below code which does work with Firefox' naga
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

  //let pos = array<vec2f, 4>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0));
  //return vec4f(pos[vertexIndex], 0.0, 1.0);
}

@fragment
fn blit(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let gidx = frame.width * u32(pos.y) + u32(pos.x);
  let col = vec4f(pow(accum[gidx].xyz / f32(frame.bouncesSpp >> 8), vec3f(0.4545)), 1.0);
  return col;
}
