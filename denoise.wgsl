/*struct Config
{
  frameData:        vec4u,          // x = bits 8-31 for width, bits 0-7 max bounces
                                    // y = bits 8-31 for height, bits 0-7 samples per pixel
                                    // z = current frame number
                                    // w = bits 8-31 for samples taken (before current frame), bits 0-7 frame's sample num
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

const SHORT_MASK = 0xffffu;

// Gaussian 5x5
const KERNEL = array<array<f32, 5>, 5>(
   array<f32, 5>(0.0030, 0.0133, 0.0219, 0.0133, 0.0030),
   array<f32, 5>(0.0133, 0.0596, 0.0983, 0.0596, 0.0133),
   array<f32, 5>(0.0219, 0.0983, 0.1621, 0.0983, 0.0219),
   array<f32, 5>(0.0133, 0.0596, 0.0983, 0.0596, 0.0133),
   array<f32, 5>(0.0030, 0.0133, 0.0219, 0.0133, 0.0030)
);

const WG_SIZE = vec3u(16, 16, 1);

@group(0) @binding(0) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(1) var<storage, read> nrmBuf: array<vec4f>;
@group(0) @binding(2) var<storage, read> posBuf: array<vec4f>;
@group(0) @binding(3) var<storage, read> dirColBuf: array<vec4f>;
@group(0) @binding(4) var<storage, read> indColBuf: array<vec4f>;
@group(0) @binding(5) var<storage, read> accumInBuf: array<vec4f>;
@group(0) @binding(6) var<storage, read_write> accumOutBuf: array<vec4f>;

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = (frame.x >> 8);
  let h = (frame.y >> 8);
  let gidx = w * globalId.y + globalId.x;
  if(gidx >= w * h) {
    return;
  }

  accumOutBuf[gidx] = vec4f(dirColBuf[gidx].xyz + indColBuf[gidx].xyz, 1.0); 
}

// Dammertz et al: Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering
/*@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = (frame.x >> 8);
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
    accumOutBuf[gidx] = accumInBuf[gidx]; // Was no hit
    return;
  }

  let nrm = nrmBuf[gidx].xyz;
  let pos = pos4.xyz;
  let acc = accumInBuf[gidx].xyz;

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
      let qacc = accumInBuf[qidx].xyz;

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

  accumOutBuf[gidx] = select(vec4f(accumInBuf[gidx].xyz, 1.0), vec4f(sum / weightSum, 1.0), weightSum != 0.0);
}*/
