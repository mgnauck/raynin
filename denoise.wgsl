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
const SIG_LUM         = 4.0;
const SIG_NRM         = 128.0;
const SIG_POS         = 1.0; // Depth
const EPS             = 0.00001;
const WG_SIZE         = vec3u(16, 16, 1);

// Attribute buf are 2x buf vec4f for nrm + pos
//  xy = octahedral encoded nrm, z = pixel pos index of last frame, w = unusued
//  xyz = pos, w = mtl id << 16 | inst id
//
// Color buffer are 2x buf vec4f for direct and indirect illum col
//
// Accum moments buf
//  xy = first and second moment direct illum
//  zw = first and second moment indirect illum
//
// Accum history buf is f32 with age
//
// Accum col/variance buf are 2x buf vec4f:
//  xyz = direct illum col, w = direct illum variance
//  xyz = indirect illum col, w = indirect illum variance

@group(0) @binding(0) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(1) var<storage, read> attrBuf: array<vec4f>;
@group(0) @binding(2) var<storage, read> lastAttrBuf: array<vec4f>;
@group(0) @binding(3) var<storage, read> colBuf: array<vec4f>;
@group(0) @binding(4) var<storage, read> accumMomInBuf: array<vec4f>;
@group(0) @binding(5) var<storage, read_write> accumMomOutBuf: array<vec4f>;
@group(0) @binding(6) var<storage, read> accumHisInBuf: array<f32>;
@group(0) @binding(7) var<storage, read_write> accumHisOutBuf: array<f32>;
@group(0) @binding(8) var<storage, read> accumColVarInBuf: array<vec4f>;
@group(0) @binding(9) var<storage, read_write> accumColVarOutBuf: array<vec4f>;

fn luminance(col: vec3f) -> f32
{
  //return dot(col, vec3f(0.2126, 0.7152, 0.0722));
  return dot(col, vec3f(0.299, 0.587, 0.114)); // Perceived
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
/*fn octToDir(oct: u32) -> vec3f
{
  let e = vec2f(bitcast<f32>(oct & 0xffff), bitcast<f32>(oct >> 16));
  let v = vec3f(e, 1.0 - abs(e.x) - abs(e.y));
  return normalize(select(v, vec3f((1.0 - abs(v.yx)) * (step(vec2f(0.0), v.xy) * 2.0 - vec2f(1.0)), v.z), v.z < 0.0));
}*/

fn octToDir(oct: vec2f) -> vec3f
{
  let v = vec3f(oct, 1.0 - abs(oct.x) - abs(oct.y));
  return normalize(select(v, vec3f((1.0 - abs(v.yx)) * (step(vec2f(0.0), v.xy) * 2.0 - vec2f(1.0)), v.z), v.z < 0.0));
}

fn ignorePrevious(lastPix: vec2i, res: vec2i, currPos: vec4f, lastPos: vec4f, currNrm: vec3f, lastNrm: vec3f) -> bool
{
  // Check out of bounds
  var ignorePrevious = any(lastPix < vec2i(0, 0)) || any(lastPix >= res);

   // Check if current and last material and instance ids match
  ignorePrevious |= bitcast<u32>(currPos.w) != bitcast<u32>(lastPos.w);

  // Consider differences of current and last normal
  ignorePrevious |= abs(dot(currNrm, lastNrm)) < 0.1;

  return ignorePrevious; 
}

// Temporally accumulate illumination + moments and calc per pixel luminance variance
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  // Current world space hit pos with w = mtl id << 16 | inst id
  let pos = attrBuf[ofs + gidx];

  // Check via mtl id if this was a no hit
  var ignorePrev = (bitcast<u32>(pos.w) >> 16) == SHORT_MASK;

  // xy = normal, zw = motion vector
  let nrmMotion = attrBuf[gidx];

  // Pixel pos in last frame
  let lpix = vec2i(i32(nrmMotion.z * f32(w)), i32(nrmMotion.w * f32(h)));

  // Pixel index of last frame (made valid in case of out of bounds)
  let lgidx = w * u32(lpix.y) + u32(lpix.x);

  // Get last frame pos in xyz and w = mtl id << 16 | inst id
  let lpos = lastAttrBuf[ofs + lgidx];

  // Get current and last normal (octrahedral encoded)
  let nrm = octToDir(nrmMotion.xy);
  let lnrm = octToDir(lastAttrBuf[lgidx].xy);

  ignorePrev |= ignorePrevious(lpix, vec2i(i32(w), i32(h)), pos, lpos, nrm, lnrm);

  // Read and increase age but keep upper bound
  let age = min(32.0, select(accumHisInBuf[gidx] + 1.0, 1.0, ignorePrev));

  // Increase weight of new samples in case of insufficient history
  let alpha = select(max(TEMPORAL_ALPHA, 1.0 / age), 1.0, ignorePrev);

  // Read color of direct and indirect illumination
  // (frame.w = 1 since we are doing fixed 1 SPP when applying the filter)
  let dcol = colBuf[      gidx].xyz / f32(frame.w);
  let icol = colBuf[ofs + gidx].xyz / f32(frame.w);

  // Calc luminance of direct and indirect ill
  let dlum = luminance(dcol);
  let ilum = luminance(icol);

  // Temporal integration using first and second raw moments of col luminance
  let mom = mix(accumMomInBuf[lgidx], vec4f(dlum, dlum * dlum, ilum, ilum * ilum), alpha);
  accumMomOutBuf[gidx] = mom;

  // Estimate temporal variance from integrated moments
  let variance = max(vec2f(0.0), mom.yw - mom.xz * mom.xz);

  // Temporal integration of direct and indirect illumination and store variance
  accumColVarOutBuf[      gidx] = vec4f(mix(accumColVarInBuf[      lgidx].xyz, dcol, alpha), variance.x);
  accumColVarOutBuf[ofs + gidx] = vec4f(mix(accumColVarInBuf[ofs + lgidx].xyz, icol, alpha), variance.y);

  // Track age (already incremented)
  accumHisOutBuf[gidx] = age;
}

// Estimation of spatial (= not enough history data) luminance variance
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  let p = vec2i(globalId.xy);
  let res = vec2i(i32(w), i32(h));

  let colVarDir = accumColVarInBuf[      gidx];
  let colVarInd = accumColVarInBuf[ofs + gidx];

  let age = accumHisInBuf[gidx];
  if(age < 4.0) {

    // Accumulated history is too short, filter moments and estimate variance
    // xy = moments direct illum, zw = moments indirect illum
    var sumMom = accumMomInBuf[gidx]; // Apply center directly
    var sumWeight = 1.0;

    for(var j=-1; j<=1; j++) {
      for(var i=-1; i<=1; i++) {

        let q = p + vec2i(i, j);
        if(any(q < vec2i(0)) || any(q >= res) || all(vec2i(i, j) == vec2i(0))) {
          continue;
        }

        sumMom += accumMomInBuf[w * u32(q.y) + u32(q.x)];
        sumWeight += 1.0;
      }
    }

    sumMom /= max(sumWeight, EPS);

    // Calc boosted variance for first frames (until temporal kicks in)
    let variance = max(vec2f(0.0), (sumMom.yw - sumMom.xz * sumMom.xz) * 4.0 / age);

    accumColVarOutBuf[      gidx] = vec4f(colVarDir.xyz, variance.x);
    accumColVarOutBuf[ofs + gidx] = vec4f(colVarInd.xyz, variance.y);
  } else {
    // Pass through
    accumColVarOutBuf[      gidx] = colVarDir;
    accumColVarOutBuf[ofs + gidx] = colVarInd;
  }
}

fn filterVariance(p: vec2i, res: vec2i) -> vec2f
{
  // 3x3 gaussian
  let kernelWeights = array<array<f32, 2>, 2>(
   array<f32, 2>(1.0 / 4.0, 1.0 / 8.0),
   array<f32, 2>(1.0 / 8.0, 1.0 / 16.0)
  );

  let ofs = u32(res.x * res.y);

  var sumVar = vec2f(0.0);
  var sumWeight = 0.0;

  for(var j=-1; j<=1; j++) {
    for(var i=-1; i<=1; i++) {

      let q = p + vec2i(i, j);
      // Disabled because we will not use the out of bounds variance values
      /*if(any(q < vec2i(0)) || any(q >= res)) {
        continue;
      }*/

      let qidx = u32(res.x * q.y + q.x);

      let w = kernelWeights[abs(i)][abs(j)];
      sumVar += w * vec2f(accumColVarInBuf[qidx].w, accumColVarInBuf[ofs + qidx].w);
      sumWeight += w;
    }
  }

  return sumVar / sumWeight;
}

// Dammertz et al: Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering
// Schied et al: Spatiotemporal Variance-Guided Filtering
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  let pos = attrBuf[ofs + gidx];

  if(bitcast<u32>(pos.w) >> 16 == SHORT_MASK) {
    // No mtl, no hit
    accumColVarOutBuf[      gidx] = accumColVarInBuf[      gidx];
    accumColVarOutBuf[ofs + gidx] = accumColVarInBuf[ofs + gidx];
    return;
  }

  let p = vec2i(globalId.xy);
  let res = vec2i(i32(w), i32(h));

  // Step size of filter increases with each iteration
  let stepSize = 1i << frame.z;

  let nrm = octToDir(attrBuf[gidx].xy);

  let colVarDir = accumColVarInBuf[      gidx];
  let colVarInd = accumColVarInBuf[ofs + gidx];

  let lumColDir = luminance(colVarDir.xyz);
  let lumColInd = luminance(colVarInd.xyz);

  // Filter variance and calc the luminance edge stopping function denominator
  let lumVarDen = SIG_LUM * sqrt(max(vec2f(0.0), vec2f(1e-10) + filterVariance(p, res)));

  // Apply center value directly
  var sumColVarDir = colVarDir;
  var sumColVarInd = colVarInd;
  var sumWeight = vec2f(1.0);

  // 5x5 gaussian
  let kernelWeights = array<f32, 3>(1.0, 2.0 / 3.0, 1.0 / 6.0);

  for(var j=-2; j<=2; j++) {
    for(var i=-2; i<=2; i++) {

      let q = p + vec2i(i, j) * stepSize;
      if(any(q < vec2i(0)) || any(q >= res) || all(vec2i(i, j) == vec2i(0))) {
        // Out of bounds or already accounted center pixel
        continue;
      }

      let qidx = w * u32(q.y) + u32(q.x);

      // Depth edge stopping function
      let qpos = attrBuf[ofs + qidx];
      let weightPos = dot(pos.xyz - qpos.xyz, pos.xyz - qpos.xyz) / SIG_POS;

      // Nrm edge stopping function
      let weightNrm = pow(max(0.0, dot(nrm, octToDir(attrBuf[qidx].xy))), SIG_NRM);
      //let weightNrm = 1.0;

      // Luminance edge stopping function
      let qcolVarDir = accumColVarInBuf[      qidx];
      let qcolVarInd = accumColVarInBuf[ofs + qidx];

      let weightLumDir = abs(lumColDir - luminance(qcolVarDir.xyz)) / lumVarDen.x;
      let weightLumInd = abs(lumColInd - luminance(qcolVarInd.xyz)) / lumVarDen.y;

      // Combined weight of edge stopping functions
      let wDir = weightNrm * exp(-weightLumDir - weightPos);
      let wInd = weightNrm * exp(-weightLumInd - weightPos);

      // Filter value is weighted by edge stopping functions to avoid 'overblurring'
      let w = kernelWeights[abs(i)] * kernelWeights[abs(j)] * vec2f(wDir, wInd);

      // Apply filter to accumulated direct/indirect color and variance
      sumColVarDir += qcolVarDir * vec4f(w.xxx, w.x * w.x);
      sumColVarInd += qcolVarInd * vec4f(w.yyy, w.y * w.y);

      sumWeight += w;
    }
  }
  
  //accumColVarOutBuf[      gidx] = sumColVarDir / vec4f(sumWeight.xxx, sumWeight.x * sumWeight.x);
  //accumColVarOutBuf[ofs + gidx] = sumColVarInd / vec4f(sumWeight.yyy, sumWeight.y * sumWeight.y);
  accumColVarOutBuf[      gidx] = select(colVarDir, sumColVarDir / vec4f(sumWeight.xxx, sumWeight.x * sumWeight.x), sumWeight.x > EPS);
  accumColVarOutBuf[ofs + gidx] = select(colVarInd, sumColVarInd / vec4f(sumWeight.yyy, sumWeight.y * sumWeight.y), sumWeight.y > EPS);
}
