/*struct Camera
{
  eye:              vec3f,
  halfTanVertFov:   f32,
  right:            vec3f,
  focDist:          f32,
  up:               vec3f,
  halfTanFocAngle:  f32,
}*/

/*struct Config
{
  frameData:        vec4u,          // x = width
                                    // y = bits 8-31 for height, bits 0-7 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

const SHORT_MASK      = 0xffffu;
const TEMPORAL_ALPHA  = 0.2;

// Gaussian 3x3
const KERNEL3x3 = array<array<f32, 3>, 3>(
   array<f32, 3>(0.075, 0.124, 0.075),
   array<f32, 3>(0.124, 0.204, 0.124),
   array<f32, 3>(0.075, 0.124, 0.075)
);

// Gaussian 5x5
// 1/16 1/4 3/8 1/4 1/16
const KERNEL5x5 = array<array<f32, 5>, 5>(
   array<f32, 5>(0.0030, 0.0133, 0.0219, 0.0133, 0.0030),
   array<f32, 5>(0.0133, 0.0596, 0.0983, 0.0596, 0.0133),
   array<f32, 5>(0.0219, 0.0983, 0.1621, 0.0983, 0.0219),
   array<f32, 5>(0.0133, 0.0596, 0.0983, 0.0596, 0.0133),
   array<f32, 5>(0.0030, 0.0133, 0.0219, 0.0133, 0.0030)
);

const WG_SIZE = vec3u(16, 16, 1);

// Attribute buf are 2x buf vec4f for nrm + pos
//  nrm = xyz, w unusued
//  pos = xyz, w = mtl id << 16 | inst id
//
// Color buffer are 2x buf vec4f for direct and indirect illum col
//
// Accum moments buf are 2x buf vec4f for direct and indirect
//  xy = first and second moment direct illum, z = age
//  xy = first and second moment indirect illum, z = age
//
// Filtered variance buf is 2x buf f32 for direct and indirect
//
// Accum col buf are 2x buf vec4f:
//  xyz = direct illum col, w = direct illum variance
//  xyz = indirect illum col, w = indirect illum variance

@group(0) @binding(0) var<uniform> lastCamera: array<vec4f, 3>;
@group(0) @binding(1) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(2) var<storage, read> attrBuf: array<vec4f>;
@group(0) @binding(3) var<storage, read> lastAttrBuf: array<vec4f>;
@group(0) @binding(4) var<storage, read> colBuf: array<vec4f>;
@group(0) @binding(5) var<storage, read> accumMomInBuf: array<vec4f>;
@group(0) @binding(6) var<storage, read_write> accumMomOutBuf: array<vec4f>;
@group(0) @binding(7) var<storage, read_write> filteredVarianceBuf: array<f32>;
@group(0) @binding(8) var<storage, read> accumColInBuf: array<vec4f>;
@group(0) @binding(9) var<storage, read_write> accumColOutBuf: array<vec4f>;

fn luminance(col: vec3f) -> f32
{
  return dot(col, vec3f(0.2126, 0.7152, 0.0722));
}

// Flipcode's Jacco Bikker: https://jacco.ompf2.com/2024/01/18/reprojection-in-a-ray-tracer/
fn calcScreenSpacePos(pos: vec3f, eye: vec4f, right: vec4f, up: vec4f, res: vec2f) -> vec2i
{
  // View plane height and width
  let vheight = 2.0 * eye.w * right.w; // 2 * halfTanVertFov * focDist
  let vwidth = vheight * res.x / res.y;

  // View plane right and down
  let vright = vwidth * right.xyz;
  let vdown = -vheight * up.xyz;

  let forward = cross(right.xyz, up.xyz);

  // Calc world space positions of view plane
  let topLeft = eye.xyz - right.w * forward - 0.5 * (vright + vdown); // right.w = focDist
  let botLeft = topLeft + vdown;
  let topRight = topLeft + vright;
  let botRight = topRight + vdown;

  // Calc normals of frustum planes
  let leftNrm = normalize(cross(botLeft - eye.xyz, topLeft - eye.xyz));
  let rightNrm = normalize(cross(topRight - eye.xyz, botRight - eye.xyz));
  let topNrm = normalize(cross(topLeft - eye.xyz, topRight - eye.xyz));
  let botNrm = normalize(cross(botRight - eye.xyz, botLeft - eye.xyz));

  let toPos = pos - eye.xyz;

  // Project pos vec to calculate horizontal ratio
  let d1x = dot(leftNrm, toPos);
  let d2x = dot(rightNrm, toPos);
  let x = d1x / (d1x + d2x);

  // Project pos vec to calculate vertical ratio
  let d1y = dot(topNrm, toPos);
  let d2y = dot(botNrm, toPos);
  let y = d1y / (d1y + d2y);

  // Find screen space pos via calculated x/y ratios
  // (Can be out of bounds)
  return vec2i(i32(res.x * x), i32(res.y * y));
}

// Temporal reprojection of current pixel to past frame
// Accumulate direct and indirect illumination and moments
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m0(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = (frame.y >> 8);
  let gidx = w * globalId.y + globalId.x;
  if(gidx >= w * h) {
    return;
  }

  let ofs = w * h;

  // Color of direct and multi-bounce indirect illumination
  let dcol = colBuf[      gidx].xyz / f32(frame.w);
  let icol = colBuf[ofs + gidx].xyz / f32(frame.w);

  let dlum = luminance(dcol);
  let ilum = luminance(icol);

  // Fetch last camera's up vec
  let lup = lastCamera[2];

  if(all(lup.xyz == vec3f(0.0))) {
    // Last up vec is not set, so it must be the first frame with no previous cam yet
    accumColOutBuf[      gidx] = vec4f(dcol, 1.0); // Direct
    accumColOutBuf[ofs + gidx] = vec4f(icol, 1.0); // Indirect
    accumMomOutBuf[      gidx] = vec4f(dlum, dlum * dlum, /* age */ 0.0, 0.0); // Direct
    accumMomOutBuf[ofs + gidx] = vec4f(ilum, ilum * ilum, /* age */ 0.0, 0.0); // Indirect
    return;
  }

  // Current world space hit pos with w = mtl id << 16 | inst id
  let pos = attrBuf[ofs + gidx];

  // Check mtl id if this was a no hit or light hit
  if((bitcast<u32>(pos.w) >> 16) == SHORT_MASK) {
    accumColOutBuf[      gidx] = vec4f(dcol, 1.0); // Direct
    accumColOutBuf[ofs + gidx] = vec4f(icol, 1.0); // Indirect
    accumMomOutBuf[      gidx] = vec4f(dlum, dlum * dlum, /* age */ 0.0, 0.0); // Direct
    accumMomOutBuf[ofs + gidx] = vec4f(ilum, ilum * ilum, /* age */ 0.0, 0.0); // Indirect
    return;
  }

  // Fetch last camera's eye pos and right vec
  let leye = lastCamera[0]; // w = halfTanVertFov
  let lri = lastCamera[1];

  // Calc where hit pos was in screen space during the last frame/camera
  let spos = calcScreenSpacePos(pos.xyz, leye, lri, lup, vec2f(f32(w), f32(h)));

  // Check if previous screen space pos is out of bounds
  var ignorePrevious = any(spos < vec2i(0i)) || any(spos >= vec2i(i32(w), i32(h)));

  // Index in last buffers
  let lgidx = w * u32(spos.y) + u32(spos.x);

  // Read last pos with mtl/inst id in w
  let lpos = lastAttrBuf[ofs + lgidx];

  // Check if current and last material and instance ids match
  ignorePrevious |= bitcast<u32>(pos.w) != bitcast<u32>(lpos.w);

  // Consider differences of current and last normal
  let nrm = attrBuf[gidx];
  let lnrm = lastAttrBuf[lgidx];
  ignorePrevious |= abs(dot(nrm, lnrm)) < 0.1;

  if(ignorePrevious) {
    accumColOutBuf[      gidx] = vec4f(dcol, 1.0); // Direct
    accumColOutBuf[ofs + gidx] = vec4f(icol, 1.0); // Indirect
    // Visualize uncorrelated areas
    //accumColOutBuf[      gidx] = vec4f(1.0, dcol.xy, 1.0); // Direct
    //accumColOutBuf[ofs + gidx] = vec4f(1.0, icol.xy, 1.0); // Indirect
    accumMomOutBuf[      gidx] = vec4f(dlum, dlum * dlum, /* age */ 0.0, 0.0); // Direct
    accumMomOutBuf[ofs + gidx] = vec4f(ilum, ilum * ilum, /* age */ 0.0, 0.0); // Indirect
    return;
  }

  // Last direct + indirect color
  let ldcol = accumColInBuf[      lgidx].xyz;
  let licol = accumColInBuf[ofs + lgidx].xyz;

  // Last direct + indirect moments
  let ldmom = accumMomInBuf[      lgidx]; // z has age, w is unused
  let limom = accumMomInBuf[ofs + lgidx];

  // Accumulate col by mixing current with past results
  accumColOutBuf[      gidx] = vec4f(mix(ldcol, dcol, TEMPORAL_ALPHA), 1.0);
  accumColOutBuf[ofs + gidx] = vec4f(mix(licol, icol, TEMPORAL_ALPHA), 1.0);

  // Similaryly, accumulate 'variance' as first and second moment of current sample's luminance
  accumMomOutBuf[      gidx] = vec4f(mix(ldmom.xy, vec2f(dlum, dlum * dlum), TEMPORAL_ALPHA), ldmom.z + 1.0, 0.0); // Increase age in z
  accumMomOutBuf[ofs + gidx] = vec4f(mix(limom.xy, vec2f(ilum, ilum * ilum), TEMPORAL_ALPHA), limom.z + 1.0, 0.0);
}

// Calc filtered variance from moments buffer
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  // TODO
  // variance (temporal) = max(0.0, mom.y - mom.x * mom.x)
  // fall back to spatial variance for age < 4
}

// Dammertz et al: Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering
/*@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = (frame.y >> 8);
  let gidx = w * globalId.y + globalId.x;
  if(gidx >= w * h) {
    return;
  }

  let lev = frame.z;
  let st = 1i << lev;

  let sigmaNrm = 0.8;
  let sigmaPos = 1.2;
  let sigmaAcc = 512.0;

  // Fetch data at current pixel p
  let pos4 = posBuf[gidx];
  let instId = bitcast<u32>(pos4.w);
  if(instId == SHORT_MASK) {
    accumColOutBuf[gidx] = accumColInBuf[gidx]; // Was no hit
    return;
  }

  let nrm = nrmBuf[gidx].xyz;
  let pos = pos4.xyz;
  let acc = accumColInBuf[gidx].xyz;

  var sum = vec3f(0.0);
  var weightSum = 0.0;
  for(var j=-2; j<=2; j++) {
    for(var i=-2; i<=2; i++) {
      let qx = i32(globalId.x) + i * st;
      let qy = i32(globalId.y) + j * st;

      // Ignore outside of image
      if(qx < 0 || qy < 0 || qx >= i32(w) || qy >= i32(h)) {
        continue;
      }

      let qidx = w * u32(qy) + u32(qx);

      // Wrong object
      let qpos4 = posBuf[qidx];
      if(bitcast<u32>(qpos4.w) != instId) {
        continue;
      }

      // Fetch data at pixel q
      let qnrm = nrmBuf[qidx].xyz;
      let qpos = qpos4.xyz;
      let qacc = accumColInBuf[qidx].xyz;

      // Calc weights for edge avoidance
      let distAccSqr = dot(acc - qacc, acc - qacc);
      let weightAcc = min(exp(-distAccSqr / sigmaAcc), 1.0);

      let distNrmSqr = dot(nrm - qnrm, nrm - qnrm);
      let weightNrm = min(exp(-distNrmSqr / sigmaNrm), 1.0);

      let distPosSqr = dot(pos - qpos, pos - qpos);
      let weightPos = min(exp(-distPosSqr / sigmaPos), 1.0);

      //let weight = KERNEL[i + 2][j + 2]; // Just atrous wavelet w/o edge avoidance
      let weight = weightAcc * weightNrm * weightPos * KERNEL[i + 2][j + 2];
      sum += qacc * weight;
      weightSum += weight;
    }
  }

  accumColOutBuf[gidx] = select(vec4f(accumColInBuf[gidx].xyz, 1.0), vec4f(sum / weightSum, 1.0), weightSum != 0.0);
}*/
