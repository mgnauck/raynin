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
  frameData:        vec4u,          // x = bits 16-31 for width, bits 0-15 for ltri cnt
                                    // y = bits 16-31 for height, bits 4-15 unused, bits 0-3 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path state cnt
  shadowRayGrid:    vec4u,          // w = shadow ray cnt
  bgColor:          vec4f           // w = ext ray cnt
}*/

const SHORT_MASK      = 0xffffu;
const ALPHA           = vec3f(0.2, 0.2, 0.2); // Alpha for dir/ind col and moments
const SIG_LUM         = 5.0;
const SIG_NRM         = 64.0;
const SIG_POS         = 1.0; // Depth
const EPS             = 0.00001;
const INF             = 3.402823466e+38;
const WG_SIZE         = vec3u(16, 16, 1);

// Attribute buf are 2x buf vec4f for nrm + pos
//  nrm buf: xy = octahedral encoded nrm, z = mtl id << 16 | inst id, w = unused
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
@group(0) @binding(2) var<storage, read> attrBuf: array<vec4f>;
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
fn calcScreenSpacePos(pos: vec3f, eye: vec4f, right: vec4f, up: vec4f, res: vec2f) -> vec2f
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

  return vec2f(res.x * d1x / (d1x + d2x), res.y * d1y / (d1y + d2y));
}

fn getDist(pix: vec2i, pos: vec3f, res: vec2i) -> f32
{
  if(any(pix < vec2i(0)) || any(pix >= res)) {
    return INF;
  }

  let idx = u32(res.x * pix.y + pix.x);
  let lpos4 = lastAttrBuf[u32(res.x * res.y) + idx];

  // Pure specular only
  if(lpos4.w == 0.0) {
    return INF;
  }

  let nrm4 = attrBuf[idx];
  let lnrm4 = lastAttrBuf[idx];

  // Mtl and inst id match
  /*if(nrm4.z != lnrm4.z) {
    return INF;
  }*/

  // Normals match
  if(dot(octToDir(nrm4.xy), octToDir(lnrm4.xy)) < 0.9) {
    return INF;
  }

  // Optimize for smallest distance between positions
  return length(pos - lpos4.xyz);
}

fn getDistanceSmall(pix: vec2f, pos: vec3f, res: vec2i) -> f32
{
  // Get distances with small 3x2 pattern
  let p = vec2i(i32(pix.x), i32(pix.y));
  let dist = vec4f(
    getDist(p, pos, res), getDist(p + vec2i(1, 0), pos, res),
    getDist(p + vec2i(0, 1), pos, res), getDist(p + vec2i(1, 1), pos, res));

  //return dot(dist, vec4f(1.0)) / 4.0;

  // Bilinear filter distances to approximate subpixel accuracy (limit temporal instabilities)
  let f = fract(pix);
  let weight = vec4f((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
  let mask = step(dist, vec4f(INF - 1.0));
  let sumWeight = dot(mask, weight);
  let sumDist = dot(mask * dist, weight);

  return select(sumDist / sumWeight, INF, sumWeight == 0.0);
}

// Voorhuis: Beyond Spatiotemporal Variance-Guided Filtering
fn searchPosition(pix: ptr<function, vec2f>, bestDist: ptr<function, f32>, pos: vec3f, stepSize: f32, res: vec2i) -> u32
{
  // Large diamond search pattern, scaled by step size
  let p = array<vec2f, 4u>(
    vec2f(pix.x - stepSize, pix.y), vec2f(pix.x + stepSize, pix.y),
    vec2f(pix.x, pix.y - stepSize), vec2f(pix.x, pix.y + stepSize));

  // Find best distance among the taps
  var bestTapNum = 0u;
  for(var i=0u; i<4u; i++) {
    let dist = getDistanceSmall(p[i], pos, res);
    if(dist < *bestDist) {
      *bestDist = dist;
      *pix = p[i];
      bestTapNum = 1u + i;
    }
  }

  return bestTapNum;
}

fn checkReproj(pos4: vec4f, lpos4: vec4f, nrm4: vec4f, lnrm4: vec4f) -> bool
{
  // Normals
  let nrm = octToDir(nrm4.xy);
  if(dot(nrm, octToDir(lnrm4.xy)) < 0.9) {
    return false;
  }

  // Depth (plane distance)
  if(abs(dot(pos4.xyz - lpos4.xyz, nrm)) > 0.05) {
    return false;
  }

  // Mtl flag
  /*if(pos4.w != lpos4.w) {
    return false;
  }*/

  // Mtl/inst id
  //return nrm4.z == lnrm4.z;
  return true;
}

fn getLastChecked(pix: vec2f, pos: vec4f, nrm: vec4f, res: vec2f, ldcol: ptr<function, vec4f>, licol: ptr<function, vec4f>, lbcol: ptr<function, vec4f>, lmom: ptr<function, vec4f>) -> bool
{
  if(all(pix >= vec2f(0.0)) && all(pix < res)) {
    let ofs = u32(res.x * res.y);
    let idx = u32(res.x * floor(pix.y) + floor(pix.x));
    let lpos = lastAttrBuf[ofs + idx];
    let lnrm = lastAttrBuf[      idx];
    if(checkReproj(pos, lpos, nrm, lnrm)) {
      *ldcol = accumColVarInBuf[             idx];
      *licol = accumColVarInBuf[       ofs + idx];
      *lbcol = accumColVarInBuf[(ofs << 1) + idx]; // Bloom
      *lmom =  accumMomInBuf[idx];
      return true;
    }
  }
  return false;
}

/*fn getLastFilteredChecked(pix: vec2f, pos: vec4f, nrm: vec4f, res: vec2f, ldcol: ptr<function, vec4f>, licol: ptr<function, vec4f>, lmom: ptr<function, vec4f>) -> bool
{
  if(all(pix >= vec2f(0.0)) && all(pix < res - vec2f(1.0))) {

    let ofs = u32(res.x * res.y);
    let w = u32(res.x);
    let p = vec2u(u32(floor(pix.x)), u32(floor(pix.y)));

    let idx0 = w * (p.y + 0) + (p.x + 0);
    let idx1 = w * (p.y + 0) + (p.x + 1);
    let idx2 = w * (p.y + 1) + (p.x + 0);
    let idx3 = w * (p.y + 1) + (p.x + 1);

    // Last position in xyz, mtl flags in w
    let lpos0 = lastAttrBuf[ofs + idx0];
    let lpos1 = lastAttrBuf[ofs + idx1];
    let lpos2 = lastAttrBuf[ofs + idx2];
    let lpos3 = lastAttrBuf[ofs + idx3];

    // Last nrm with xy = encoded nrm, z = mtl/inst id, w = unused
    let lnrm0 = lastAttrBuf[idx0];
    let lnrm1 = lastAttrBuf[idx1];
    let lnrm2 = lastAttrBuf[idx2];
    let lnrm3 = lastAttrBuf[idx3];

    // Bilinear filter
    let f = fract(pix);
    let w0 = select(0.0, (1.0 - f.x) * (1.0 - f.y), checkReproj(pos, lpos0, nrm, lnrm0));
    let w1 = select(0.0,        f.x  * (1.0 - f.y), checkReproj(pos, lpos1, nrm, lnrm1));
    let w2 = select(0.0, (1.0 - f.x) *         f.y, checkReproj(pos, lpos2, nrm, lnrm2));
    let w3 = select(0.0,        f.x  *         f.y, checkReproj(pos, lpos3, nrm, lnrm3));

    var sumW = w0 + w1 + w2 + w3;

    if(sumW > EPS) {

      sumW = 1.0 / sumW;

      *ldcol = (w0 * accumColVarInBuf[      idx0] + w1 * accumColVarInBuf[      idx1] + w2 * accumColVarInBuf[      idx2] + w3 * accumColVarInBuf[      idx3]) * sumW;
      *licol = (w0 * accumColVarInBuf[ofs + idx0] + w1 * accumColVarInBuf[ofs + idx1] + w2 * accumColVarInBuf[ofs + idx2] + w3 * accumColVarInBuf[ofs + idx3]) * sumW;
      *lmom  = (w0 *    accumMomInBuf[      idx0] + w1 *    accumMomInBuf[      idx1] + w2 *    accumMomInBuf[      idx2] + w3 *    accumMomInBuf[      idx3]) * sumW;

      return true;
    }
  }

  return false;
}*/

// Temporally accumulate illumination + moments and calc per pixel luminance variance
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x >> 16;
  let h = frame.y >> 16;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let res = vec2f(f32(w), f32(h));
  let gidx = w * globalId.y + globalId.x;

  let lup = lastCamera[2];

  // Flag if this is the first frame
  var ignorePrev = all(lup.xyz == vec3f(0.0));

   // Nrm encoded in xy, mtl/inst id in z, w unused
  let nrm4 = attrBuf[gidx];

  // Check if no hit
  ignorePrev |= (bitcast<u32>(nrm4.z) >> 16) == SHORT_MASK;

  // Last direct/indirect illumination and moments
  var ldcol = vec4f(0.0);
  var licol = vec4f(0.0);
  var lbcol = vec4f(0.0);
  var lmom  = vec4f(0.0);

  // Position in xyz and material flags in w
  let pos4 = attrBuf[ofs + gidx];

  // Reproject current position to last frame
  var pix = calcScreenSpacePos(pos4.xyz, lastCamera[0], lastCamera[1], lup, res);

  // If there was a pure specular bounce, the reprojected position can be wrong
  if(pos4.w == 1.0) {

    let lidx = select(u32(res.x * pix.y + pix.x), w * h, any(pix < vec2f(0.0)) || any(pix >= res));

    // First candidate is current pos reprojected to last frame
    let cand1 = pix;
    let dist1 = length(pos4.xyz - lastAttrBuf[ofs + lidx].xyz);

    // Second candidate is current pos as is taken to last frame
    let cand2 = vec2f(f32(globalId.x), f32(globalId.y));
    let dist2 = length(pos4.xyz - lastAttrBuf[ofs + gidx].xyz);

    // Decide which one is better as starting point
    let useCand2 = dist2 < dist1 || lidx == w * h;
    var dist = select(dist1, dist2, useCand2);
    var cand = select(cand1, cand2, useCand2);

    // Begin to search from the evaluated position
    var stepSize = 5.0;
    var cnt = 0u;
    loop {
      let tapNum = searchPosition(&cand, &dist, pos4.xyz, stepSize, vec2i(i32(res.x), i32(res.y)));
      // If center tap was best pos (i.e. there was no change), reduce the step size
      stepSize *= select(1.0, 0.5, tapNum == 0);
      cnt++;
      if(cnt >= 20 || stepSize < 0.05) {
        break;
      }
    }

    // Re-assign result from search
    pix = cand;
  }

  pix += vec2f(0.5, 0.5);

  ignorePrev |= !ignorePrev && !getLastChecked(pix, pos4, nrm4, res, &ldcol, &licol, &lbcol, &lmom);
  //ignorePrev |= !ignorePrev && !getLastFilteredChecked(pix, pos4, nrm4, res, &ldcol, &licol, &lmom);

  // Read and increase age but keep upper bound
  let age = min(32.0, select(accumHisInBuf[gidx] + 1.0, 1.0, ignorePrev));

  // Increase weight of new samples in case of insufficient history
  var alpha = select(max(ALPHA, vec3f(1.0 / age)), vec3f(1.0), ignorePrev);
  //alpha *= select(1.0, 0.5, pos4.w == 1.0 && !ignorePrev);

  // Read color of direct and indirect illumination
  // (frame.w = 1 since we are doing fixed 1 SPP when applying the filter)
  let dcol = colBuf[      gidx].xyz / f32(frame.w);
  let icol = colBuf[ofs + gidx].xyz / f32(frame.w);

  // Calc luminance of direct and indirect ill
  let dlum = luminance(dcol);
  let ilum = luminance(icol);

  // Temporal integration using first and second raw moments of col luminance
  var mom = vec4f(0.0);
  if(age <= 4.0) {
    mom = mix(lmom, vec4f(dlum, dlum, ilum, ilum), 1.0 / age);
  } else {
    mom = mix(lmom, vec4f(dlum, dlum * dlum, ilum, ilum * ilum), alpha.z);
  }
  accumMomOutBuf[gidx] = mom;

  // Temporal integration using first and second raw moments of col luminance
  /*let mom = mix(lmom, vec4f(dlum, dlum * dlum, ilum, ilum * ilum), alpha.z);
  if(age <= 4.0) {
    accumMomOutBuf[gidx] = mix(lmom, vec4f(dlum, dlum, ilum, ilum), 1.0 / age);
  } else {
    accumMomOutBuf[gidx] = mom;
  }*/

  // Estimate temporal variance from integrated moments
  let variance = max(vec2f(0.0), mom.yw - mom.xz * mom.xz);

  // Temporal integration of direct and indirect illumination and store variance
  accumColVarOutBuf[      gidx] = vec4f(mix(ldcol.xyz, dcol, alpha.x), variance.x);
  accumColVarOutBuf[ofs + gidx] = vec4f(mix(licol.xyz, icol, alpha.y), variance.y);

  //if(luminance(dcol + icol) > 0.5) {
  if((bitcast<u32>(nrm4.z) >> 16) == SHORT_MASK) {
    accumColVarOutBuf[(ofs << 1) + gidx] = vec4f(mix(lbcol.xyz, dcol + icol, ALPHA.x), 1.0);
  } else {
    accumColVarOutBuf[(ofs << 1) + gidx] = vec4f(0.0, 0.0, 0.0, 1.0);
  }

  // Track age (already incremented)
  accumHisOutBuf[gidx] = age;
}

fn calcEdgeStoppingWeights(ofs: u32, idx: u32, pos: vec3f, nrm: vec3f, qnrm: vec3f, lumColDir: f32, lumColInd: f32, lumColDirQ: f32, lumColIndQ: f32, sigLumDir: f32, sigLumInd: f32) -> vec2f
{
  // Nrm edge stopping function
  let weightNrm = pow(max(0.0, dot(nrm, qnrm)), SIG_NRM);
  //let weightNrm = 1.0;

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
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x >> 16;
  let h = frame.y >> 16;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  // xy = encoded nrm, z = mtl/inst id, w = last idx
  let nrm4 = attrBuf[gidx];

  let age = accumHisInBuf[gidx];
  if(age < 4.0 && bitcast<u32>(nrm4.z) >> 16 != SHORT_MASK) {

    // Accumulated history is too short, filter illumination/moments and estimate variance
    let p = vec2i(globalId.xy);
    let res = vec2i(i32(w), i32(h));

    let pos = attrBuf[ofs + gidx].xyz;
    let nrm = octToDir(nrm4.xy);

    let colVarDir = accumColVarInBuf[             gidx];
    let colVarInd = accumColVarInBuf[       ofs + gidx];
    let colBloom  = accumColVarInBuf[(ofs << 1) + gidx];

    // xy = moments direct illum, zw = moments indirect illum
    let mom = accumMomInBuf[gidx];

    let lumColDir = luminance(colVarDir.xyz);
    let lumColInd = luminance(colVarInd.xyz);

    // Apply center value directly
    var sumColVarDir  = colVarDir.xyz;
    var sumColVarInd  = colVarInd.xyz;
    var sumColBloom   = colBloom.xyz;
    var sumMom        = mom; 
    var sumWeight     = vec3f(1.0);

    for(var j=-1; j<=1; j++) {
      for(var i=-1; i<=1; i++) {

        let q = p + vec2i(i, j);
        if(any(q < vec2i(0)) || any(q >= res) || all(vec2i(i, j) == vec2i(0))) {
          continue;
        }

        let qidx = w * u32(q.y) + u32(q.x);

        // xy = encoded nrm, z = mtl/inst id, w = last idx
        let qnrm4 = attrBuf[qidx];

        let qcolVarDir = accumColVarInBuf[             qidx].xyz;
        let qcolVarInd = accumColVarInBuf[       ofs + qidx].xyz;
        let qcolBloom  = accumColVarInBuf[(ofs << 1) + qidx].xyz;

        let qmom = accumMomInBuf[qidx];

        let weight = select(0.0, 1.0, qnrm4.z == nrm4.z) * calcEdgeStoppingWeights(
          //ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd, luminance(qcolVarDir), luminance(qcolVarInd), SIG_LUM, SIG_LUM);
          ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd, qmom.x, qmom.z, SIG_LUM, SIG_LUM);

        sumColVarDir += qcolVarDir * weight.x;
        sumColVarInd += qcolVarInd * weight.y;
        sumColBloom  += qcolBloom;
          
        sumMom += qmom * vec4f(weight.xx, weight.yy);

        sumWeight += vec3f(weight, 1.0);
      }
    }

    sumColVarDir = select(colVarDir.xyz, sumColVarDir / sumWeight.x, sumWeight.x > EPS);
    sumColVarInd = select(colVarInd.xyz, sumColVarInd / sumWeight.y, sumWeight.y > EPS);
    sumColBloom  = select(colBloom.xyz, sumColBloom / sumWeight.z, sumWeight.z > EPS);
    
    //sumWeight = max(sumWeight, vec2f(EPS));
    sumMom /= vec4f(sumWeight.xx, sumWeight.yy);
    
    // Calc boosted variance for first frames (until temporal kicks in)
    let variance = max(vec2f(0.0), vec2f(sumMom.yw - sumMom.xz * sumMom.xz)) * 4.0 / age;

    accumColVarOutBuf[             gidx] = vec4f(sumColVarDir, variance.x);
    accumColVarOutBuf[       ofs + gidx] = vec4f(sumColVarInd, variance.y);
    accumColVarOutBuf[(ofs << 1) + gidx] = vec4f(sumColBloom, 1.0);

  } else {
    // History is enough or no hit, pass through
    accumColVarOutBuf[             gidx] = accumColVarInBuf[             gidx];
    accumColVarOutBuf[       ofs + gidx] = accumColVarInBuf[       ofs + gidx];
    accumColVarOutBuf[(ofs << 1) + gidx] = accumColVarInBuf[(ofs << 1) + gidx]; // Bloom
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

// Filter direct illum variance only
/*fn filterDIVariance(p: vec2i, res: vec2i) -> vec2f
{
  // 3x3 gaussian
  let kernelWeights = array<array<f32, 2>, 2>(
   array<f32, 2>(1.0 / 4.0, 1.0 / 8.0),
   array<f32, 2>(1.0 / 8.0, 1.0 / 16.0)
  );

  let ofs = u32(res.x * res.y);

  var sumVar = 0.0;
  var sumWeight = 0.0;

  for(var j=-1; j<=1; j++) {
    for(var i=-1; i<=1; i++) {
      let q = p + vec2i(i, j);
      let qidx = u32(res.x * q.y + q.x);
      let weight = kernelWeights[abs(i)][abs(j)];
      sumVar += weight * accumColVarInBuf[qidx].w;
      sumWeight += weight;
    }
  }

  return vec2f(sumVar / sumWeight, accumColVarInBuf[ofs + u32(res.x * p.y + p.x)].w);
}*/

// Dammertz et al: Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering
// Schied, Frenetic et al: Spatiotemporal Variance-Guided Filtering
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m2(@builtin(global_invocation_id) globalId: vec3u)
{
  let frame = config[0];
  let w = frame.x >> 16;
  let h = frame.y >> 16;
  if(any(globalId.xy >= vec2u(w, h))) {
    return;
  }

  let ofs = w * h;
  let gidx = w * globalId.y + globalId.x;

  // xy = encoded nrm, z = mtl/inst id, w = last idx
  let nrm4 = attrBuf[gidx];

  if(bitcast<u32>(nrm4.z) >> 16 == SHORT_MASK) {
    // No mtl, no hit
    accumColVarOutBuf[             gidx] = accumColVarInBuf[             gidx];
    accumColVarOutBuf[       ofs + gidx] = accumColVarInBuf[       ofs + gidx];
    accumColVarOutBuf[(ofs << 1) + gidx] = accumColVarInBuf[(ofs << 1) + gidx]; // Bloom
    return;
  }

  let p = vec2i(globalId.xy);
  let res = vec2i(i32(w), i32(h));

  // Step size of filter increases with each iteration
  let stepSize = 1i << frame.z;

  let pos = attrBuf[ofs + gidx].xyz;
  let nrm = octToDir(nrm4.xy);

  let colVarDir = accumColVarInBuf[             gidx];
  let colVarInd = accumColVarInBuf[       ofs + gidx];
  let colBloom  = accumColVarInBuf[(ofs << 1) + gidx].xyz;

  let lumColDir = luminance(colVarDir.xyz);
  let lumColInd = luminance(colVarInd.xyz);

  // Filter variance and calc the luminance edge stopping function denominator
  let lumVarDen = SIG_LUM * sqrt(max(vec2f(0.0), vec2f(1e-10) + filterVariance(p, res)));
  //let lumVarDen = SIG_LUM * sqrt(max(vec2f(0.0), vec2f(1e-10) + filterDIVariance(p, res)));

  // Apply center value directly
  var sumColVarDir = colVarDir;
  var sumColVarInd = colVarInd;
  var sumColBloom  = colBloom.xyz;
  var sumWeight = vec3f(1.0);

  // 5x5 gaussian
  let kernelWeights = array<f32, 3>(1.0, 2.0 / 3.0, 1.0 / 6.0);

  for(var j=-2; j<=2; j++) {
    for(var i=-2; i<=2; i++) {

      let q = p + vec2i(i, j) * stepSize;
      let qb = p + vec2i(i, j) * (1i + i32(frame.z));
      if(any(q < vec2i(0)) || any(q >= res) || all(vec2i(i, j) == vec2i(0))) {
        // Out of bounds or already accounted for center pixel
        continue;
      }

      let qidx = w * u32(q.y) + u32(q.x);
      let qbidx = w * u32(qb.y) + u32(qb.x); // Pixel for bloom

      // xy = encoded nrm, z = mtl/inst id, w = last idx
      let qnrm4 = attrBuf[qidx];

      let qcolVarDir = accumColVarInBuf[             qidx];
      let qcolVarInd = accumColVarInBuf[       ofs + qidx];
      //let qcolBloom  = accumColVarInBuf[(ofs << 1) + qidx].xyz;
      let qcolBloom  = accumColVarInBuf[(ofs << 1) + qbidx].xyz;

      // Gaussian influenced by edge stopping weights
      let kw = kernelWeights[abs(i)] * kernelWeights[abs(j)];
      let weight = select(0.0, 1.0, qnrm4.z == nrm4.z) * kw *
        calcEdgeStoppingWeights(ofs, qidx, pos, nrm, octToDir(qnrm4.xy), lumColDir, lumColInd,
          luminance(qcolVarDir.xyz), luminance(qcolVarInd.xyz), lumVarDen.x, lumVarDen.y);

      // Apply filter to accumulated direct/indirect color and variance
      // as per section 4.3 of the paper (squared for variance)
      sumColVarDir += qcolVarDir * vec4f(weight.xxx, weight.x * weight.x);
      sumColVarInd += qcolVarInd * vec4f(weight.yyy, weight.y * weight.y);
      sumColBloom  += qcolBloom * kw;

      sumWeight += vec3f(weight, kw);
    }
  }
  
  accumColVarOutBuf[             gidx] = select(colVarDir, sumColVarDir / vec4f(sumWeight.xxx, sumWeight.x * sumWeight.x), sumWeight.x > EPS);
  accumColVarOutBuf[       ofs + gidx] = select(colVarInd, sumColVarInd / vec4f(sumWeight.yyy, sumWeight.y * sumWeight.y), sumWeight.y > EPS);
  accumColVarOutBuf[(ofs << 1) + gidx] = vec4f(select(colBloom, sumColBloom / sumWeight.z, sumWeight.z > EPS), 1.0);
}
