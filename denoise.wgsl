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
                                    // y = bits 8-31 for height, bits 4-7 unused, bits 0-3 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

const SHORT_MASK      = 0xffffu;
const SPECULAR_BOUNCE = 4u; // Had a specular bounce
const ALPHA           = vec3f(0.05, 0.1, 0.2); // Alpha for dir/ind col and moments
const SIG_LUM         = 4.0;
const SIG_NRM         = 128.0;
const SIG_POS         = 1.0; // Depth
const EPS             = 0.00001;
const WG_SIZE         = vec3u(16, 16, 1);

// Attribute buf are 2x buf vec4f for nrm + pos
//  nrm buf: xy = octahedral encoded nrm, z = mtl id << 16 | inst id, w = pixel pos index of last frame
//  pos buf: xyz = pos, w = flags
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

@group(0) @binding(0) var<uniform> lastCamera: array<vec4f, 3>;
@group(0) @binding(1) var<storage, read> config: array<vec4u, 4>;
@group(0) @binding(2) var<storage, read_write> attrBuf: array<vec4f>;
@group(0) @binding(3) var<storage, read> lastAttrBuf: array<vec4f>;
@group(0) @binding(4) var<storage, read> colBuf: array<vec4f>;
@group(0) @binding(5) var<storage, read> accumMomInBuf: array<vec4f>;
@group(0) @binding(6) var<storage, read_write> accumMomOutBuf: array<vec4f>;
@group(0) @binding(7) var<storage, read> accumHisInBuf: array<f32>;
@group(0) @binding(8) var<storage, read_write> accumHisOutBuf: array<f32>;
@group(0) @binding(9) var<storage, read> accumColVarInBuf: array<vec4f>;
@group(0) @binding(10) var<storage, read_write> accumColVarOutBuf: array<vec4f>;

fn luminance(col: vec3f) -> f32
{
  //return dot(col, vec3f(0.2126, 0.7152, 0.0722));
  return dot(col, vec3f(0.299, 0.587, 0.114)); // Perceived
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
fn octToDir(oct: vec2f) -> vec3f
{
  let v = vec3f(oct, 1.0 - abs(oct.x) - abs(oct.y));
  return normalize(select(v, vec3f((1.0 - abs(v.yx)) * (step(vec2f(0.0), v.xy) * 2.0 - vec2f(1.0)), v.z), v.z < 0.0));
}

/*fn octToDirQuant(oct: u32) -> vec3f
{
  let e = vec2f(bitcast<f32>(oct & 0xffff), bitcast<f32>(oct >> 16));
  let v = vec3f(e, 1.0 - abs(e.x) - abs(e.y));
  return normalize(select(v, vec3f((1.0 - abs(v.yx)) * (step(vec2f(0.0), v.xy) * 2.0 - vec2f(1.0)), v.z), v.z < 0.0));
}*/

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

  // Project pos vec to calculate vertical ratio
  let d1y = dot(topNrm, toPos);
  let d2y = dot(botNrm, toPos);

  return vec2i(i32(res.x * d1x / (d1x + d2x)), i32(res.y * d1y / (d1y + d2y)));
}

fn reproject(pos: vec3f, w: u32, h: u32) -> u32
{
  // Get last camera values
  let lup = lastCamera[2];

  let res = vec2i(i32(w), i32(h));

  // Calc where pos was in screen space during the last frame/camera
  let last = select(
    calcScreenSpacePos(pos, lastCamera[0], lastCamera[1], lup, vec2f(f32(w), f32(h))),
    res, // Out of bounds
    all(lup.xyz == vec3f(0.0))); // Check if last cam is valid

  return select(
    w * u32(last.y) + u32(last.x),
    w * h, // Out of bounds
    any(last < vec2i(0i)) || any(last >= res)); // Check if within viewport
}

// Calc screen space pos/idx in last frame via reprojection
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

  // Position in xyz and material flags in w
  let pos = attrBuf[ofs + gidx];

  // Calc current pixel index in last frame
  // In case pixel index is out of bounds, it will be set to w*h
  var lidx: u32;
  if((bitcast<u32>(pos.w) & SPECULAR_BOUNCE) == 0) {
    // Saw no specular bounce, simply reproject to last frame
    lidx = reproject(pos.xyz, w, h);
  } else {
    // Search for the pixel
    lidx = gidx;
  }

  // Write pixel index in last frame to nrm.w
  attrBuf[gidx].w = bitcast<f32>(lidx);
}

// Temporally accumulate illumination + moments and calc per pixel luminance variance
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

  // xy = encoded normal, z = mtl id << 16 | inst id, w = pixel index in last frame (reprojected)
  let nrm4 = attrBuf[gidx];

  // Check via mtl id if this was a no hit
  var ignorePrev = (bitcast<u32>(nrm4.z) >> 16) == SHORT_MASK;

  // Get pixel index in last frame (in case of out of bounds this was set to w * h)
  let lgidx = bitcast<u32>(nrm4.w);
  ignorePrev |= lgidx == w * h;

   // Check if current and last material and instance ids match
  let lnrm4 = lastAttrBuf[lgidx];
  ignorePrev |= bitcast<u32>(nrm4.z) != bitcast<u32>(lnrm4.z);

  // Consider differences of current and last normal
  ignorePrev |= abs(dot(octToDir(nrm4.xy), octToDir(lnrm4.xy))) < 0.1;

  // Read and increase age but keep upper bound
  let age = min(32.0, select(accumHisInBuf[gidx] + 1.0, 1.0, ignorePrev));

  // Increase weight of new samples in case of insufficient history
  let alpha = select(max(ALPHA, vec3f(1.0 / age)), vec3f(1.0), ignorePrev);

  // Read color of direct and indirect illumination
  // (frame.w = 1 since we are doing fixed 1 SPP when applying the filter)
  let dcol = colBuf[      gidx].xyz / f32(frame.w);
  let icol = colBuf[ofs + gidx].xyz / f32(frame.w);

  // Calc luminance of direct and indirect ill
  let dlum = luminance(dcol);
  let ilum = luminance(icol);

  // Temporal integration using first and second raw moments of col luminance
  let mom = mix(accumMomInBuf[lgidx], vec4f(dlum, dlum * dlum, ilum, ilum * ilum), alpha.z);
  if(age <= 4.0) {
    accumMomOutBuf[gidx] = mix(accumMomInBuf[lgidx], vec4f(dlum, dlum, ilum, ilum), 1.0 / age);
  } else {
    accumMomOutBuf[gidx] = mom;
  }

  // Estimate temporal variance from integrated moments
  let variance = max(vec2f(0.0), mom.yw - mom.xz * mom.xz);

  // Temporal integration of direct and indirect illumination and store variance
  accumColVarOutBuf[      gidx] = vec4f(mix(accumColVarInBuf[      lgidx].xyz, dcol, alpha.x), variance.x);
  accumColVarOutBuf[ofs + gidx] = vec4f(mix(accumColVarInBuf[ofs + lgidx].xyz, icol, alpha.y), variance.y);

  // Track age (already incremented)
  accumHisOutBuf[gidx] = age;
}

fn calcEdgeStoppingWeights(ofs: u32, idx: u32, pos: vec3f, nrm: vec3f, qnrm: vec3f, lumColDir: f32, lumColInd: f32, lumColDirQ: f32, lumColIndQ: f32, sigLumDir: f32, sigLumInd: f32) -> vec2f
{
  // Nrm edge stopping function
  let weightNrm = pow(max(0.0, dot(nrm, qnrm)), SIG_NRM);

  // Depth edge stopping function
  let qpos = attrBuf[ofs + idx].xyz;
  //let weightPos = dot(pos - qpos, pos.xyz - qpos.xyz) / SIG_POS;
  let weightPos = abs(dot(pos - qpos, nrm)) / SIG_POS; // Plane distance

  // Luminance edge stopping function
  let weightLumDir = abs(lumColDir - lumColDirQ) / sigLumDir;
  let weightLumInd = abs(lumColInd - lumColIndQ) / sigLumInd;

  // Combined weight of edge stopping functions
  let wDir = weightNrm * exp(-weightLumDir - weightPos);
  let wInd = weightNrm * exp(-weightLumInd - weightPos);

  return vec2f(wDir, wInd);
}

// Estimation of spatial luminance variance (when history is not enough)
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

  // xy = encoded nrm, z = mtl/inst id, w = last idx
  let nrm4 = attrBuf[gidx];

  let age = accumHisInBuf[gidx];
  if(age < 4.0 && (bitcast<u32>(nrm4.z) >> 16) != SHORT_MASK) {

    // Accumulated history is too short, filter illumination/moments and estimate variance
    let p = vec2i(globalId.xy);
    let res = vec2i(i32(w), i32(h));

    let pos = attrBuf[ofs + gidx].xyz;
    let nrm = octToDir(nrm4.xy);

    let colVarDir = accumColVarInBuf[      gidx];
    let colVarInd = accumColVarInBuf[ofs + gidx];

    // xy = moments direct illum, zw = moments indirect illum
    let mom = accumMomInBuf[gidx];

    let lumColDir = luminance(colVarDir.xyz);
    let lumColInd = luminance(colVarInd.xyz);

    // Apply center value directly
    var sumColVarDir  = colVarDir.xyz;
    var sumColVarInd  = colVarInd.xyz;
    var sumMom        = mom; 
    var sumWeight     = vec2f(1.0);

    for(var j=-1; j<=1; j++) {
      for(var i=-1; i<=1; i++) {

        let q = p + vec2i(i, j);
        if(any(q < vec2i(0)) || any(q >= res) || all(vec2i(i, j) == vec2i(0))) {
          continue;
        }

        let qidx = w * u32(q.y) + u32(q.x);

        // xy = encoded nrm, z = mtl/inst id, w = last idx
        let qnrm4 = attrBuf[qidx];

        let qcolVarDir = accumColVarInBuf[      qidx].xyz;
        let qcolVarInd = accumColVarInBuf[ofs + qidx].xyz;

        let qmom = accumMomInBuf[qidx];

        let weight = select(0.0, 1.0, qnrm4.z == nrm4.z) * calcEdgeStoppingWeights(
          //ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd, luminance(qcolVarDir), luminance(qcolVarInd), SIG_LUM, SIG_LUM);
          ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd, qmom.x, qmom.z, SIG_LUM, SIG_LUM);

        sumColVarDir += qcolVarDir * weight.x;
        sumColVarInd += qcolVarInd * weight.y;
          
        sumMom += qmom * vec4f(weight.xx, weight.yy);

        sumWeight += weight;
      }
    }

    sumColVarDir = select(colVarDir.xyz, sumColVarDir / sumWeight.x, sumWeight.x > EPS);
    sumColVarInd = select(colVarInd.xyz, sumColVarInd / sumWeight.y, sumWeight.y > EPS);
    
    //sumWeight = max(sumWeight, vec2f(EPS));
    sumMom /= vec4f(sumWeight.xx, sumWeight.yy);
    
    // Calc boosted variance for first frames (until temporal kicks in)
    let variance = max(vec2f(0.0), vec2f(sumMom.yw - sumMom.xz * sumMom.xz)) * 4.0 / age;

    accumColVarOutBuf[      gidx] = vec4f(sumColVarDir, variance.x);
    accumColVarOutBuf[ofs + gidx] = vec4f(sumColVarInd, variance.y);

  } else {
    // History is enough or no hit, pass through
    accumColVarOutBuf[      gidx] = accumColVarInBuf[      gidx];
    accumColVarOutBuf[ofs + gidx] = accumColVarInBuf[ofs + gidx];
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
      let qidx = u32(res.x * q.y + q.x);
      let weight = kernelWeights[abs(i)][abs(j)];
      sumVar += weight * vec2f(accumColVarInBuf[qidx].w, accumColVarInBuf[ofs + qidx].w);
      sumWeight += weight;
    }
  }

  return sumVar / sumWeight;
}

// Dammertz et al: Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering
// Schied et al: Spatiotemporal Variance-Guided Filtering
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m3(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x;
  let h = frame.y >> 8;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  // xy = encoded nrm, z = mtl/inst id, w = last idx
  let nrm4 = attrBuf[gidx];

  if(bitcast<u32>(nrm4.z) >> 16 == SHORT_MASK) {
    // No mtl, no hit
    accumColVarOutBuf[      gidx] = accumColVarInBuf[      gidx];
    accumColVarOutBuf[ofs + gidx] = accumColVarInBuf[ofs + gidx];
    return;
  }

  let p = vec2i(globalId.xy);
  let res = vec2i(i32(w), i32(h));

  // Step size of filter increases with each iteration
  let stepSize = 1i << frame.z;

  let pos = attrBuf[ofs + gidx].xyz;
  let nrm = octToDir(nrm4.xy);

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
        // Out of bounds or already accounted for center pixel
        continue;
      }

      let qidx = w * u32(q.y) + u32(q.x);

      // xy = encoded nrm, z = mtl/inst id, w = last idx
      let qnrm4 = attrBuf[qidx];

      let qcolVarDir = accumColVarInBuf[      qidx];
      let qcolVarInd = accumColVarInBuf[ofs + qidx];

      // Gaussian influenced by edge stopping weights
      let weight = select(0.0, 1.0, qnrm4.z == nrm4.z) * kernelWeights[abs(i)] * kernelWeights[abs(j)] *
        calcEdgeStoppingWeights(ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd,
          luminance(qcolVarDir.xyz), luminance(qcolVarInd.xyz), lumVarDen.x, lumVarDen.y);

      // Apply filter to accumulated direct/indirect color and variance
      // as per section 4.3 of the paper (squared for variance)
      sumColVarDir += qcolVarDir * vec4f(weight.xxx, weight.x * weight.x);
      sumColVarInd += qcolVarInd * vec4f(weight.yyy, weight.y * weight.y);

      sumWeight += weight;
    }
  }
  
  accumColVarOutBuf[      gidx] = select(colVarDir, sumColVarDir / vec4f(sumWeight.xxx, sumWeight.x * sumWeight.x), sumWeight.x > EPS);
  accumColVarOutBuf[ofs + gidx] = select(colVarInd, sumColVarInd / vec4f(sumWeight.yyy, sumWeight.y * sumWeight.y), sumWeight.y > EPS);
}
