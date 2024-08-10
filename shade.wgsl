struct Config
{
  width:            u32,            // Bits 8-31 for width, bits 0-7 max bounces
  height:           u32,            // Bit 8-31 for height, bits 0-7 samples per pixel
  frame:            u32,            // Current frame number
  samplesTaken:     u32,            // Bits 8-31 for samples taken (before current frame), bits 0-7 frame's sample num
  pathCnt:          u32,
  extRayCnt:        atomic<u32>,
  shadowRayCnt:     atomic<u32>,
  pad0:             u32,
  gridDimPath:      vec4u,
  gridDimSRay:      vec4u,
  bgColor:          vec4f           // w is unused
}

struct Mtl
{
  col:              vec3f,          // Base color (diff col of non-metallics, spec col of metallics)
  metallic:         f32,            // Appearance range from dielectric to conductor (0 - 1)
  roughness:        f32,            // Perfect reflection to completely diffuse (0 - 1)
  ior:              f32,            // Index of refraction
  refractive:       f32,            // Flag if material refracts
  emissive:         f32             // Flag if material is emissive
}

struct Inst
{
  invTransform:     mat3x4f,
  id:               u32,            // (mtl override id << 16) | (inst id & 0xffff)
  data:             u32,            // See comment on data in inst.h
  pad0:             u32,
  pad1:             u32
}

struct Tri
{
  v0:               vec3f,
  mtl:              u32,            // (mtl id & 0xffff)
  v1:               vec3f,
  ltriId:           u32,            // Set only if tri has light emitting material
  v2:               vec3f,
  pad0:             f32,
  n0:               vec3f,
  pad1:             f32,
  n1:               vec3f,
  pad2:             f32,
  n2:               vec3f,
  pad3:             f32
}

struct LTri
{
  v0:               vec3f,
  triId:            u32,            // Original tri id of the mesh (w/o inst data ofs)
  v1:               vec3f,
  pad0:             f32,
  v2:               vec3f,
  area:             f32,
  nrm:              vec3f,
  power:            f32,            // Precalculated product of area and emission
  emission:         vec3f,
  pad1:             f32
}

struct ShadowRay
{
  ori:              vec3f,
  pidx:             u32,            // Pixel index where to deposit contribution
  dir:              vec3f,
  dist:             f32,
  contribution:     vec3f,
  pad0:             f32
}

struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, bounce num in bits 0-7
}

// Scene data handling
const SHORT_MASK          = 0xffffu;
const MTL_OVERRIDE_BIT    = 0x80000000u; // Bit 32
const MESH_SHAPE_MASK     = 0x3fffffffu; // Bits 30-0

// General constants
const EPS                 = 0.001;
const INF                 = 3.402823466e+38;
const PI                  = 3.141592;
const TWO_PI              = 6.283185;
const INV_PI              = 1.0 / PI;

const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> materials: array<Mtl, 1024>; // One mtl per inst
@group(0) @binding(1) var<uniform> instances: array<Inst, 1024>; // Uniform buffer max is 64k bytes by default
@group(0) @binding(2) var<storage, read> tris: array<Tri>;
@group(0) @binding(3) var<storage, read> ltris: array<LTri>;
@group(0) @binding(4) var<storage, read> hits: array<vec4f>;
@group(0) @binding(5) var<storage, read> pathStatesIn: array<PathState>;
@group(0) @binding(6) var<storage, read_write> config: Config;
@group(0) @binding(7) var<storage, read_write> pathStatesOut: array<PathState>;
@group(0) @binding(8) var<storage, read_write> shadowRays: array<ShadowRay>;
@group(0) @binding(9) var<storage, read_write> accum: array<vec4f>;

// State of rng seed
var<private> seed: u32;

fn rand() -> f32
{
  // xorshift32
  seed ^= seed << 13u;
  seed ^= seed >> 17u;
  seed ^= seed <<  5u;
  return bitcast<f32>(0x3f800000 | (seed >> 9)) - 1.0;
}

fn rand4() -> vec4f
{
  return vec4f(rand(), rand(), rand(), rand());
}

fn maxComp3(v: vec3f) -> f32
{
  return max(v.x, max(v.y, v.z));
}

fn toMat3x3(m: mat3x4f) -> mat3x3f
{
  return mat3x3f(m[0].xyz, m[1].xyz, m[2].xyz);
}

fn toMat4x4(m: mat3x4f) -> mat4x4f
{
  return mat4x4f(m[0], m[1], m[2], vec4f(0, 0, 0, 1));
}

// Duff et al: Building an Orthonormal Basis, Revisited
fn createONB(n: vec3f) -> mat3x3f
{
  var r: mat3x3f;
  let s = select(-1.0, 1.0, n.z >= 0.0); // = copysign
  let a = -1.0 / (s + n.z);
  let b = n.x * n.y * a;
  r[0] = vec3f(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
  r[1] = n;
  r[2] = vec3f(b, s + n.y * n.y * a, -n.y);
  return r;
}

// Parallelogram method, https://mathworld.wolfram.com/TrianglePointPicking.html
fn sampleBarycentric(r: vec2f) -> vec3f
{
  if(r.x + r.y > 1.0) {
    return vec3f(1.0 - r.x, 1.0 - r.y, -1.0 + r.x + r.y);
  }

  return vec3f(r, 1 - r.x - r.y);
}

// https://mathworld.wolfram.com/DiskPointPicking.html
fn sampleDisk(r: vec2f) -> vec2f
{
  let radius = sqrt(r.x);
  let theta = 2 * PI * r.y;
  return vec2f(cos(theta), sin(theta)) * radius;
}

fn sampleHemisphereCos(r: vec2f) -> vec3f
{
  // Sample disc at base of hemisphere uniformly
  let disk = sampleDisk(r);

  // Project samples up to the hemisphere
  return vec3f(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), disk.y);
}

fn normalToWorldSpace(nrm: vec3f, inst: Inst) -> vec3f
{
  // Transform normal to world space with transpose of inverse
  return normalize(nrm * transpose(toMat3x3(inst.invTransform)));
}

fn calcTriNormal(bary: vec2f, inst: Inst, tri: Tri) -> vec3f
{
  let nrm = tri.n1 * bary.x + tri.n2 * bary.y + tri.n0 * (1.0 - bary.x - bary.y);
  return normalToWorldSpace(nrm, inst);
}

fn luminance(col: vec3f) -> f32
{
  return dot(col, vec3f(0.2126, 0.7152, 0.0722));
}

fn getRoughness(mtl: Mtl) -> f32
{
  return saturate(mtl.roughness * mtl.roughness - EPS) + EPS;
}

fn mtlToF0(mtl: Mtl) -> vec3f
{
  var f0: f32;
  f0 = (1.0 - mtl.ior) / (1.0 + mtl.ior);
  f0 *= f0;
 
  // For metallic materials we use the base color, otherwise the ior
  return mix(vec3f(f0), mtl.col, mtl.metallic);
}

// https://en.wikipedia.org/wiki/Schlick's_approximation
fn fresnelSchlick(cosTheta: f32, f0: vec3f) -> vec3f
{
  return f0 + (vec3f(1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

// Boksansky: Crash Cource in BRDF Implementation
// Walter et al: Microfacet Models for Refraction through Rough Surfaces
// https://agraphicsguynotes.com/posts/sample_microfacet_brdf/
fn distributionGGX(n: vec3f, h: vec3f, alpha: f32) -> f32
{
  let NoH = dot(n, h);
  let alphaSqr = alpha * alpha;
  let NoHSqr = NoH * NoH;
  let den = NoHSqr * alphaSqr + (1.0 - NoHSqr);
  return (step(EPS, NoH) * alphaSqr) / (PI * den * den);
}

fn geometryPartialGGX(v: vec3f, n: vec3f, h: vec3f, alpha: f32) -> f32
{
  var VoHSqr = dot(v, h);
  let chi = step(EPS, VoHSqr / dot(n, v));
  VoHSqr = VoHSqr * VoHSqr;
  let tan2 = (1.0 - VoHSqr) / VoHSqr;
  return (chi * 2.0) / (1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

fn sampleGGX(n: vec3f, r0: vec2f, alpha: f32) -> vec3f
{
  // Sample the microfacet normal
  let cosTheta = sqrt((1.0 - r0.x) / ((alpha * alpha - 1.0) * r0.x + 1.0));
  let sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  let phi = TWO_PI * r0.y;
  return normalize(createONB(n) * vec3f(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta));
}

fn sampleGGXPdf(n: vec3f, m: vec3f, alpha: f32) -> f32
{
  return distributionGGX(n, m, alpha) * abs(dot(n, m));
}

fn sampleReflection(mtl: Mtl, wo: vec3f, n: vec3f, m: vec3f, wi: ptr<function, vec3f>) -> f32
{
  if(dot(wo, m) <= 0.0) {
    return 0.0;
  }

  // Reflect at microfacet normal
  *wi = reflect(-wo, m);
  return sampleReflectionPdf(mtl, wo, n, *wi);
}

fn sampleReflectionPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  let h = sign(dot(n, wo)) * normalize(wo + wi);
  let dwhDwi = 4.0 * abs(dot(wo, h));
  return sampleGGXPdf(n, h, getRoughness(mtl)) / dwhDwi;
}

fn evalReflection(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  // Modulate by sign so that half vector works for front and back side
  let h = sign(dot(n, wo)) * normalize(wo + wi);

  let roughness = getRoughness(mtl);
  
  let distr = distributionGGX(n, h, roughness);
  let geom = geometryPartialGGX(wo, n, h, roughness) * geometryPartialGGX(wi, n, h, roughness);

  return distr * geom / max(0.05, 4.0 * abs(dot(n, wo)) * abs(dot(n, wi)));
}

fn sampleRefraction(mtl: Mtl, wo: vec3f, n: vec3f, m: vec3f, wi: ptr<function, vec3f>) -> f32
{
  if(dot(wo, m) <= 0.0) {
    return 0.0;
  }

  // Refract at microfacet normal
  let etaRatio = select(1 / mtl.ior, mtl.ior, dot(wo, n) < 0);
  *wi = refract(-wo, m, etaRatio);
  if(all(*wi == vec3f(0))) {
    return 0.0; // TIR
  }
 
  return sampleRefractionPdf(mtl, wo, n, *wi);
}

fn sampleRefractionPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> f32
{
  let etaI = select(1.0, mtl.ior, dot(wo, n) < 0); // incoming
  let etaT = select(mtl.ior, 1.0, dot(wo, n) < 0); // transmitted
  var h = -normalize(etaI * wo + etaT * wi);

  let denom = etaI * dot(wo, h) + etaT * dot(wi, h);
  let dwhDwi = etaT * etaT * abs(dot(wi, h)) / (denom * denom); 

  return sampleGGXPdf(n, h, getRoughness(mtl)) * dwhDwi;
}

fn evalRefraction(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f) -> vec3f
{
  let etaI = select(1.0, mtl.ior, dot(wo, n) < 0);
  let etaT = select(mtl.ior, 1.0, dot(wo, n) < 0);
  var h = -normalize(etaI * wo + etaT * wi);

  let WIoH = dot(wi, h);
  let WOoH = dot(wo, h);

  let roughness = getRoughness(mtl);
  
  let distr = distributionGGX(n, h, roughness);
  let geom = geometryPartialGGX(wo, n, h, roughness) * geometryPartialGGX(wi, n, h, roughness);
  
  let denom = etaI * WOoH + etaT * WIoH;
  let a = etaT / denom;

  return mtl.col * distr * geom * a * a * abs(WIoH * WOoH / (dot(n, wo) * dot(n, wi)));
}

fn sampleDiffuse(n: vec3f, r0: vec2f, wi: ptr<function, vec3f>) -> f32
{
  *wi = createONB(n) * sampleHemisphereCos(r0);
  return sampleDiffusePdf(n, *wi);
}

fn sampleDiffusePdf(n: vec3f, wi: vec3f) -> f32
{
  return max(0.0, dot(n, wi)) * INV_PI;
}

fn evalDiffuse(mtl: Mtl, n: vec3f, wi: vec3f) -> vec3f
{
  return (1.0 - mtl.metallic) * mtl.col * INV_PI;
}

fn sampleMaterial(mtl: Mtl, wo: vec3f, n: vec3f, r0: vec3f, wi: ptr<function, vec3f>, fres: ptr<function, vec3f>, isSpecular: ptr<function, bool>, pdf: ptr<function, f32>) -> bool
{
  let m = sign(dot(wo, n)) * sampleGGX(n, r0.xy, getRoughness(mtl));
  *fres = fresnelSchlick(dot(wo, m), mtlToF0(mtl));
  let p = luminance(*fres);

  if(r0.z < p) {
    *isSpecular = true;
    *pdf = sampleReflection(mtl, wo, n, m, wi) * p;
  } else {
    *isSpecular = false;
    if(mtl.refractive > 0.0) { // Do not write as select, likely error in naga
      *pdf = sampleRefraction(mtl, wo, n, m, wi) * (1.0 - p);
    } else {
      *pdf = sampleDiffuse(n, r0.xy, wi) * (1.0 - p);
    }
  }

  return *pdf > 0.0;
}

fn sampleMaterialCombinedPdf(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: vec3f) -> f32
{
  let f = luminance(fres);
  let otherPdf = select(sampleDiffusePdf(n, wi), sampleRefractionPdf(mtl, wo, n, wi), mtl.refractive > 0.0);
  return otherPdf * (1.0 - f) + sampleReflectionPdf(mtl, wo, n, wi) * f;
}

fn evalMaterial(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: vec3f, isSpecular: bool) -> vec3f
{
  if(isSpecular) {
    return evalReflection(mtl, wo, n, wi) * fres;
  } else {
    return select(evalDiffuse(mtl, n, wi), evalRefraction(mtl, wo, n, wi), mtl.refractive > 0.0) * (vec3f(1) - fres);
  }
}

fn evalMaterialCombined(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: ptr<function, vec3f>) -> vec3f
{
  if(dot(n, wi) > 0.0) {
    let h = normalize(wo + wi);
    *fres = fresnelSchlick(dot(wo, h), mtlToF0(mtl));
    let otherBsdf = select(evalDiffuse(mtl, n, wi), evalRefraction(mtl, wo, n, wi), mtl.refractive > 0.0);
    return otherBsdf * (vec3f(1) - *fres) + evalReflection(mtl, wo, n, wi) * (*fres);
  }

  return vec3f(0);
}

fn sampleLTrisUniform(r0: vec3f, pos: vec3f, nrm: vec3f, lnrm: ptr<function, vec3f>, ldir: ptr<function, vec3f>, ldistInv: ptr<function, f32>, emission: ptr<function, vec3f>, pdf: ptr<function, f32>) -> bool
{
  let bc = sampleBarycentric(r0.xy);

  let ltriCnt = arrayLength(&ltris);
  let ltriId = u32(floor(r0.z * f32(ltriCnt)));

  let ltri = &ltris[ltriId];
  let lpos = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;

  var lightDir = lpos - pos;
  *ldistInv = inverseSqrt(dot(lightDir, lightDir));
  lightDir *= *ldistInv;

  *ldir = lightDir;
  *lnrm = (*ltri).nrm;

  var visible = dot(lightDir, *lnrm) < 0; // Front side of ltri only
  visible &= dot(lightDir, nrm) > 0; // Not facing (behind)

  *emission = (*ltri).emission;

  *pdf = 1.0 / ((*ltri).area * f32(ltriCnt));

  return visible;
}

fn sampleLTriUniformPdf(ltriId: u32) -> f32
{
  return 1.0 / (ltris[ltriId].area * f32(arrayLength(&ltris)));
}

fn calcLTriContribution(pos: vec3f, nrm: vec3f, lpos: vec3f, lnrm: vec3f, lightPower: f32, ldir: ptr<function, vec3f>, ldistInv: ptr<function, f32>) -> f32
{
  var lightDir = lpos - pos;
  let invDist = inverseSqrt(dot(lightDir, lightDir));
  lightDir *= invDist;

  *ldir = lightDir;
  *ldistInv = invDist;

  // Inclination between light normal and light dir
  // (how much the light faces the light dir)
  let lnDotL = max(0.0, -dot(lnrm, lightDir));

  // Inclination between surface normal and light dir
  // (how much the surface aligns with the light dir or if the light is behind)
  let nDotL = max(0.0, dot(nrm, lightDir));

  // Inclination angles scale our measure of light power / dist^2
  return lnDotL * nDotL * lightPower * invDist * invDist;
}

// Talbot et al: Importance Resampling for Global Illumination
fn sampleLTrisRIS(pos: vec3f, nrm: vec3f, lnrm: ptr<function, vec3f>, ldir: ptr<function, vec3f>, ldistInv: ptr<function, f32>, emission: ptr<function, vec3f>, pdf: ptr<function, f32>) -> bool
{
  let sampleCnt = 8u;
  
  // Source pdf is uniform selection of ltris
  let ltriCnt = f32(arrayLength(&ltris));
  let sourcePdf = 1.0 / f32(ltriCnt);

  // Selected sample tracking
  var totalWeight = 0.0;
  var sampleTargetPdf: f32;
  var area: f32;

  for(var i=0u; i<sampleCnt; i++) {

    let r = rand4();
    let bc = sampleBarycentric(r.xy);

    // Sample a candidate ltri from all ltris with 'cheap' source pdf
    let ltri = &ltris[u32(floor(r.z * ltriCnt))];
    let lpos = (*ltri).v0 * bc.x + (*ltri).v1 * bc.y + (*ltri).v2 * bc.z;

    var ldirCand: vec3f;
    var ldistInvCand: f32;

    // Re-sample the selected sample to approximate the more accurate target pdf
    let targetPdf = calcLTriContribution(pos, nrm, lpos, (*ltri).nrm, (*ltri).power, &ldirCand, &ldistInvCand);
    let risWeight = targetPdf / sourcePdf;
    totalWeight += risWeight;

    if(r.w < risWeight / totalWeight) {
      // Store data of our latest accepted sample/candidate
      *lnrm = (*ltri).nrm;
      *ldir = ldirCand;
      *ldistInv = ldistInvCand;
      *emission = (*ltri).emission;
      area = (*ltri).area;
      // Track pdf of the selected sample
      sampleTargetPdf = targetPdf;
    }
  }

  *pdf = (f32(sampleCnt) * sampleTargetPdf) / (area * totalWeight);

  return *pdf > 0.0;
}

// Lessig: The Area Formulation of Light Transport
fn geomSolidAngle(ldir: vec3f, surfNrm: vec3f, ldistInv: f32) -> f32
{
  return abs(dot(ldir, surfNrm)) * ldistInv * ldistInv;
}

fn finalizeHit(ori: vec3f, dir: vec3f, hit: vec4f, pos: ptr<function, vec3f>, nrm: ptr<function, vec3f>, ltriId: ptr<function, u32>, mtl: ptr<function, Mtl>)
{
  // Note: hit.xyzw = vec4f(t, u, v, (inst.id << 16) | tri id & 0xffff)
  let inst = instances[bitcast<u32>(hit.w) & SHORT_MASK];
 
  let ofs = inst.data & MESH_SHAPE_MASK;
  let tri = tris[ofs + (bitcast<u32>(hit.w) >> 16)];

  // Either use the material id from the triangle or the material override from the instance
  *mtl = materials[select(tri.mtl & SHORT_MASK, inst.id >> 16, (inst.data & MTL_OVERRIDE_BIT) > 0)];
  *pos = ori + hit.x * dir;
  *nrm = calcTriNormal(hit.yz, inst, tri);
  *ltriId = tri.ltriId; // Is not set if not emissive

  // Flip normal if backside, except if we hit a ltri or refractive mtl
  *nrm *= select(-1.0, 1.0, dot(-dir, *nrm) > 0 || (*mtl).emissive > 0.0 || (*mtl).refractive > 0.0);
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let gidx = config.gridDimPath.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= config.pathCnt) {
    return;
  }

  let w = config.width;
  let ofs = (w >> 8) * (config.height >> 8) * (config.samplesTaken & 0xff);
  let hit = hits[gidx];
  let pstate = pathStatesIn[ofs + gidx];
  let pidx = pstate.pidx;
  var throughput = select(pstate.throughput, vec3f(1.0), (pidx & 0xff) == 0); // Avoid mem access

  // No hit, terminate path
  if(hit.x == INF) {
    accum[pidx >> 8] += vec4f(throughput * config.bgColor.xyz, 1.0);
    return;
  }

  var pos: vec3f;
  var nrm: vec3f;
  var ltriId: u32;
  var mtl: Mtl;
  finalizeHit(pstate.ori, pstate.dir, hit, &pos, &nrm, &ltriId, &mtl);

  // Hit light via material direction
  if(mtl.emissive > 0.0) {
    // Lights emit from front side only
    if(dot(-pstate.dir, nrm) > 0) {
      // Bounces > 0
      if((pidx & 0xff) > 0) {
        // Secondary ray hit light, apply MIS
        let ldir = pos - pstate.ori;
        let ldistInv = inverseSqrt(dot(ldir, ldir));
        let pdf = pstate.pdf * geomSolidAngle(ldir * ldistInv, nrm, ldistInv);
        let lpdf = sampleLTriUniformPdf(ltriId);
        let weight = pdf / (pdf + lpdf);
        accum[pidx >> 8] += vec4f(throughput * weight * mtl.col, 1.0);
      } else {
        // Primary ray hit light
        accum[pidx >> 8] += vec4f(throughput * mtl.col, 1.0);
      }
    }
    // Terminate ray after light hit (lights do not reflect)
    return;
  }

  seed = pstate.seed;
  let r0 = rand4();

  // Russian roulette
  // Terminate with prob inverse to throughput
  let p = min(0.95, maxComp3(throughput));
  if(/*(pidx & 0xff) > 0 &&*/ r0.w > p) {
    // Terminate path due to russian roulette
    return;
  }
  // Account for bias introduced by path termination
  // Boost surviving paths by their probability to be terminated
  throughput *= 1.0 / p;

  // Sample lights directly (NEE)
  var lnrm: vec3f;
  var ldir: vec3f;
  var ldistInv: f32;
  var emission: vec3f;
  var lpdf: f32;
  //if(sampleLTrisUniform(rand4().xyz, pos, nrm, &lnrm, &ldir, &ldistInv, &emission, &lpdf)) {
  if(sampleLTrisRIS(pos, nrm, &lnrm, &ldir, &ldistInv, &emission, &lpdf)) {
    // Veach/Guibas: Optimally Combining Sampling Techniques for Monte Carlo Rendering
    let gsa = geomSolidAngle(ldir, lnrm, ldistInv);
    var fres: vec3f;
    let bsdf = evalMaterialCombined(mtl, -pstate.dir, nrm, ldir, &fres);
    let bsdfPdf = sampleMaterialCombinedPdf(mtl, -pstate.dir, nrm, ldir, fres);
    let weight = lpdf / (lpdf + bsdfPdf * gsa);
    // Init shadow ray
    let sidx = atomicAdd(&config.shadowRayCnt, 1u);
    shadowRays[sidx].ori = pos;
    shadowRays[sidx].dir = ldir;
    shadowRays[sidx].dist = 1.0 / ldistInv - 2.0 * EPS;
    shadowRays[sidx].contribution = throughput * bsdf * gsa * weight * emission * saturate(dot(nrm, ldir)) / lpdf;
    shadowRays[sidx].pidx = pidx >> 8;
  }

  // Reached max bounces (encoded in width lower 8 bits), terminate path
  if((pidx & 0xff) == (w & 0xff) - 1) {
    return;
  }

  // Sample material
  var wi: vec3f;
  var fres: vec3f;
  var isSpecular: bool;
  var pdf: f32;
  if(!sampleMaterial(mtl, -pstate.dir, nrm, r0.xyz, &wi, &fres, &isSpecular, &pdf)) {
    return;
  }

  // Apply bsdf
  throughput *= evalMaterial(mtl, -pstate.dir, nrm, wi, fres, isSpecular) * abs(dot(nrm, wi)) / pdf;

  // Get compacted index into the path state buffer
  let gidxNext = ofs + atomicAdd(&config.extRayCnt, 1u);

  // Init next path segment 
  pathStatesOut[gidxNext].seed = seed;
  pathStatesOut[gidxNext].throughput = throughput;
  pathStatesOut[gidxNext].pdf = pdf;
  pathStatesOut[gidxNext].ori = pos;
  pathStatesOut[gidxNext].dir = wi;
  pathStatesOut[gidxNext].pidx = pidx + 1; // Keep pixel index but increase bounce num in lowest 8 bits
}
