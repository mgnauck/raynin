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

@group(0) @binding(0) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(1) var<storage, read> colBuf: array<vec4f>;

const pos = array<vec2f, 4u>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0)); // Not in naga
//var<private> pos: array<vec2f, 4u> = array<vec2f, 4u>(vec2f(-1.0, 1.0), vec2f(-1.0), vec2f(1.0), vec2f(1.0, -1.0));

@vertex
fn vm(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
  return vec4f(pos[vertexIndex], 0.0, 1.0);
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
fn filmicToneACES(x: vec3f) -> vec3f
{
  let a = 2.51;
  let b = 0.03;
  let c = 2.43;
  let d = 0.59;
  let e = 0.14;
  return saturate(x * (a * x + vec3(b)) / (x * (c * x + vec3(d)) + vec3(e)));
}

@fragment
fn m(@builtin(position) pos: vec4f) -> @location(0) vec4f
{
  let frame = config[0];
  let w = frame.x >> 16;
  let h = frame.y >> 16;
  let gidx = w * u32(pos.y) + u32(pos.x);
  
  var fcol = colBuf[gidx].xyz / f32(frame.w);

  // Vignette
  //var q = vec2f(pos.x / f32(w), pos.y / f32(h));
  //q *= vec2f(1.0) - q.yx;
  //fcol *= pow(q.x * q.y * 5.0, 0.1);

  // Tone map/gamma
  fcol = filmicToneACES(fcol);
  fcol = pow(fcol, vec3f(0.4545));
 
  return vec4f(fcol, 1.0);
}
