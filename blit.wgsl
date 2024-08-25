@group(0) @binding(0) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(1) var<storage, read> accumBuf: array<vec4f>;

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

@fragment
fn m(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let frame = config[0];
  let gidx = (frame.x >> 8) * u32(pos.y) + u32(pos.x); // width
  let col = vec4f(pow(accumBuf[gidx].xyz, vec3f(0.4545)), 1.0);
  return col;
}
