/*struct Camera
{
  eye:              vec3f,
  halfTanVertFov:   f32,
  right:            vec3f,
  focDist:          f32,
  up:               vec3f,
  halfTanFocAngle:  f32,
}*/

struct Config
{
  frameData:        vec4u,          // x = width
                                    // y = bits 8-31 for height, bits 0-7 max bounces
                                    // z = frame number
                                    // w = sample number
  pathStateGrid:    vec4u,          // w = path cnt
  shadowRayGrid:    vec3u,
  shadowRayCnt:     atomic<u32>,
  bgColor:          vec3f,
  extRayCnt:        atomic<u32>
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

/*struct Inst
{
  invTransform:     mat3x4f,
  id:               u32,            // (mtl override id << 16) | (inst id & 0xffff)
  data:             u32,            // See comment on data in inst.h
  pad0:             u32,
  pad1:             u32
}*/

/*struct TriNrm
{
  n0:               vec3f,
  mtl:              u32,            // (mtl id & 0xffff)
  n1:               vec3f,
  ltriId:           u32,            // Set only if tri has light emitting material
  n2:               vec3f,
  pad3:             f32
}*/

/*struct LTri
{
  v0:               vec3f,
  nx:               f32,
  v1:               vec3f,
  ny:               f32,
  v2:               vec3f,
  nz:               f32,
  emission:         vec3f,
  area:             f32
}*/

/*struct ShadowRay
{
  ori:              vec3f,
  pidx:             u32,            // Pixel index where to deposit contribution
  dir:              vec3f,
  dist:             f32,
  contribution:     vec3f,
  pad0:             f32
}*/

/*struct PathState
{
  throughput:       vec3f,
  pdf:              f32,
  ori:              vec3f,
  seed:             u32,
  dir:              vec3f,
  pidx:             u32             // Pixel idx in bits 8-31, bounce num in bits 0-7
}*/

// Scene data handling
const SHORT_MASK          = 0xffffu;
const MTL_OVERRIDE_BIT    = 0x80000000u; // Bit 32
const INST_DATA_MASK      = 0x3fffffffu; // Bits 31-0

// General constants
const EPS                 = 0.001;
const INF                 = 3.402823466e+38;
const PI                  = 3.141592;
const TWO_PI              = 6.283185;
const INV_PI              = 1.0 / PI;

const WG_SIZE             = vec3u(16, 16, 1);

@group(0) @binding(0) var<uniform> lastCamera: array<vec4f, 3>;
@group(0) @binding(1) var<uniform> materials: array<Mtl, 1024>; // One mtl per inst
@group(0) @binding(2) var<uniform> instances: array<vec4f, 1024 * 4>; // Uniform buffer max is 64kb by default
@group(0) @binding(3) var<storage, read> triNrms: array<vec4f>;
@group(0) @binding(4) var<storage, read> ltris: array<vec4f>;
@group(0) @binding(5) var<storage, read> hits: array<vec4f>;
@group(0) @binding(6) var<storage, read> pathStatesIn: array<vec4f>;
@group(0) @binding(7) var<storage, read_write> config: Config;
@group(0) @binding(8) var<storage, read_write> pathStatesOut: array<vec4f>;
@group(0) @binding(9) var<storage, read_write> shadowRays: array<vec4f>;
@group(0) @binding(10) var<storage, read_write> attrBuf: array<vec4f>;
@group(0) @binding(11) var<storage, read_write> colBuf: array<vec4f>;

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

fn sampleMaterial(mtl: Mtl, wo: vec3f, n: vec3f, r0: vec3f, wi: ptr<function, vec3f>, fres: ptr<function, vec3f>, isSpecRefl: ptr<function, bool>, pdf: ptr<function, f32>) -> bool
{
  let m = sign(dot(wo, n)) * sampleGGX(n, r0.xy, getRoughness(mtl));
  *fres = fresnelSchlick(dot(wo, m), mtlToF0(mtl));
  let p = luminance(*fres);

  if(r0.z < p) {
    *isSpecRefl = true;
    *pdf = sampleReflection(mtl, wo, n, m, wi) * p;
  } else {
    *isSpecRefl = false;
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

fn evalMaterial(mtl: Mtl, wo: vec3f, n: vec3f, wi: vec3f, fres: vec3f, isSpecRefl: bool) -> vec3f
{
  if(isSpecRefl) {
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

/*fn sampleLTrisUniform(r0: vec3f, pos: vec3f, nrm: vec3f, lnrm: ptr<function, vec3f>, ldir: ptr<function, vec3f>, ldistInv: ptr<function, f32>, emission: ptr<function, vec3f>, pdf: ptr<function, f32>) -> bool
{
  let bc = sampleBarycentric(r0.xy);

  let ltriCnt = arrayLength(&ltris) >> 2; // TODO Use actual ltri cnt because of disabled ones!
  let ltriId = u32(floor(r0.z * f32(ltriCnt)));

  let v0 = ltris[(ltriId << 2) + 0]; // w = lnrm.x
  let v1 = ltris[(ltriId << 2) + 1]; // w = lnrm.y
  let v2 = ltris[(ltriId << 2) + 2]; // w = lnrm.z
  let em = ltris[(ltriId << 2) + 3]; // w = area

  let lpos = v0.xyz * bc.x + v1.xyz * bc.y + v2.xyz * bc.z;

  var lightDir = lpos - pos;
  *ldistInv = inverseSqrt(dot(lightDir, lightDir));
  lightDir *= *ldistInv;

  *ldir = lightDir;
  *lnrm = vec3f(v0.w, v1.w, v2.w);

  var visible = dot(lightDir, *lnrm) < 0; // Front side of ltri only
  visible &= dot(lightDir, nrm) > 0; // Not facing (behind)

  *emission = em.xyz;

  *pdf = 1.0 / (em.w * f32(ltriCnt));

  return visible;
}*/

fn sampleLTriUniformPdf(ltriId: u32) -> f32
{
  let area = ltris[(ltriId << 2) + 3].w;
  return 1.0 / (area * f32(arrayLength(&ltris) >> 2)); // TODO Use actual ltri cnt because of disabled ones!
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
  let ltriCnt = f32(arrayLength(&ltris) >> 2); // TODO Use actual ltri cnt because of disabled ones!
  let sourcePdf = 1.0 / f32(ltriCnt);

  // Selected sample tracking
  var totalWeight = 0.0;
  var sampleTargetPdf: f32;
  var area: f32;

  for(var i=0u; i<sampleCnt; i++) {

    let r = rand4();
    let bc = sampleBarycentric(r.xy);

    // Sample a candidate ltri from all ltris with 'cheap' source pdf
    let ltriId = u32(floor(r.z * ltriCnt));
    let v0 = ltris[(ltriId << 2) + 0]; // w = lnrm.x
    let v1 = ltris[(ltriId << 2) + 1]; // w = lnrm.y
    let v2 = ltris[(ltriId << 2) + 2]; // w = lnrm.z
    let em = ltris[(ltriId << 2) + 3]; // w = area

    let lpos = v0.xyz * bc.x + v1.xyz * bc.y + v2.xyz * bc.z;

    var ldirCand: vec3f;
    var ldistInvCand: f32;

    let ln = vec3f(v0.w, v1.w, v2.w);

    // Re-sample the selected sample to approximate the more accurate target pdf
    let targetPdf = calcLTriContribution(pos, nrm, lpos, ln, em.w * (em.x + em.y + em.z), &ldirCand, &ldistInvCand);
    let risWeight = targetPdf / sourcePdf;
    totalWeight += risWeight;

    if(r.w < risWeight / totalWeight) {
      // Store data of our latest accepted sample/candidate
      *lnrm = ln;
      *ldir = ldirCand;
      *ldistInv = ldistInvCand;
      *emission = em.xyz;
      area = em.w;
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

fn calcTriNormal(bary: vec2f, n0: vec3f, n1: vec3f, n2: vec3f, m: mat4x4f) -> vec3f
{
  let nrm = n1 * bary.x + n2 * bary.y + n0 * (1.0 - bary.x - bary.y);
  return normalize((vec4f(nrm, 0.0) * transpose(m)).xyz);
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
/*fn dirToOct(dir: vec3f) -> u32
{
  let p = dir.xy * (1.0 / dot(abs(dir), vec3f(1.0)));
  let e = select((1.0 - abs(p.yx)) * (step(vec2f(0.0), p) * 2.0 - vec2f(1.0)), p, dir.z > 0.0);
  return (bitcast<u32>(quantizeToF16(e.y)) << 16) + (bitcast<u32>(quantizeToF16(e.x)));
}*/

fn dirToOct(dir: vec3f) -> vec2f
{
  let v = dir.xy * (1.0 / dot(abs(dir), vec3f(1.0)));
  let e = select((1.0 - abs(v.yx)) * (step(vec2f(0.0), v) * 2.0 - vec2f(1.0)), v, dir.z > 0.0);
  return e;
}

fn finalizeHit(ori: vec3f, dir: vec3f, hit: vec4f, pos: ptr<function, vec3f>, nrm: ptr<function, vec3f>, ltriId: ptr<function, u32>, mtl: ptr<function, Mtl>) -> u32
{
  // hit = vec4f(t, u, v, (tri id << 16) | inst id & 0xffff)
  let instOfs = (bitcast<u32>(hit.w) & SHORT_MASK) << 2;

  // Inst inverse transform
  let m = mat4x4f(  instances[instOfs + 0],
                    instances[instOfs + 1],
                    instances[instOfs + 2],
                    vec4f(0.0, 0.0, 0.0, 1.0));

  // Inst id + inst data
  let data = instances[instOfs + 3];

  let mtlInstId = bitcast<u32>(data.x); // mtl id << 16 | inst id
  let instData = bitcast<u32>(data.y); // mtl override bit << 31 | tri buf ofs

  let ofs = instData & INST_DATA_MASK;
  let triOfs = (ofs + (bitcast<u32>(hit.w) >> 16)) * 3;
  let n0 = triNrms[triOfs + 0]; // w = mtl id
  let n1 = triNrms[triOfs + 1]; // w = ltri id
  let n2 = triNrms[triOfs + 2];

  // Either use the material id from the triangle or the material override from the instance
  let mtlId = select(bitcast<u32>(n0.w) & SHORT_MASK, mtlInstId >> 16, (instData & MTL_OVERRIDE_BIT) > 0);
  *mtl = materials[mtlId];
  *pos = ori + hit.x * dir;
  *nrm = calcTriNormal(hit.yz, n0.xyz, n1.xyz, n2.xyz, m);
  *ltriId = bitcast<u32>(n1.w); // Not set if not emissive

  // Flip normal if backside, except if we hit a ltri or refractive mtl
  *nrm *= select(-1.0, 1.0, dot(-dir, *nrm) > 0 || (*mtl).emissive > 0.0 || (*mtl).refractive > 0.0);

  // Consumed by function that renders the attribute buffer for primary rays
  return mtlInstId;
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

  // Project pos vec to calculate vertical ratio
  let d1y = dot(topNrm, toPos);
  let d2y = dot(botNrm, toPos);

  return vec2i(i32(res.x * d1x / (d1x + d2x)), i32(res.y * d1y / (d1y + d2y)));
}

// Reproject current position to last frame and render attribute buffer
@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m(@builtin(global_invocation_id) globalId: vec3u)
{
  let pathStateGrid = config.pathStateGrid;
  let gidx = pathStateGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= pathStateGrid.w) { // path cnt
    return;
  }

  let frame = config.frameData;
  let w = frame.x;
  let h = frame.y >> 8;
  let ofs = w * h;
  let hit = hits[gidx];
  let ori = pathStatesIn[ofs + gidx];
  let dir = pathStatesIn[(ofs << 1) + gidx];
  let pidx = bitcast<u32>(dir.w);

  let res = vec2i(i32(w), i32(h));

  if(hit.x == INF) {
    // No hit means zero nrm/motion/pos
    attrBuf[pidx >> 8] = vec4f(0.0, 0.0, bitcast<f32>(w * h), 0.0);
    let noHit = SHORT_MASK << 16; // Tint wants this as extra variable otherwise this is NAN?
    attrBuf[ofs + (pidx >> 8)] = vec4f(0.0, 0.0, 0.0, bitcast<f32>(noHit));
    return;
  }

  var pos: vec3f;
  var nrm: vec3f;
  var ltriId: u32;
  var mtl: Mtl;
  let mtlInstId = finalizeHit(ori.xyz, dir.xyz, hit, &pos, &nrm, &ltriId, &mtl);

  // Get last camera values
  let lup = lastCamera[2];

  // Calc where hit pos was in screen space during the last frame/camera
  let last = select(
    calcScreenSpacePos(pos, lastCamera[0], lastCamera[1], lup, vec2f(f32(w), f32(h))),
    res, // Out of bounds
    all(lup.xyz == vec3f(0.0))); // Check if last cam is valid

  let lastIdx = select(
    w * u32(last.y) + u32(last.x),
    w * h,
    any(last < vec2i(0i)) || any(last >= res));

  attrBuf[pidx >> 8] = vec4f(dirToOct(nrm), bitcast<f32>(lastIdx), 0.0);
  attrBuf[ofs + (pidx >> 8)] = vec4f(pos, bitcast<f32>(mtlInstId));
}

@compute @workgroup_size(WG_SIZE.x, WG_SIZE.y, WG_SIZE.z)
fn m1(@builtin(global_invocation_id) globalId: vec3u)
{
  let pathStateGrid = config.pathStateGrid;
  let gidx = pathStateGrid.x * WG_SIZE.x * globalId.y + globalId.x;
  if(gidx >= pathStateGrid.w) { // path cnt
    return;
  }

  let frame = config.frameData;
  let h = frame.y;
  let ofs = frame.x * (h >> 8);
  let hit = hits[gidx];
  let ori = pathStatesIn[ofs + gidx];
  let dir = pathStatesIn[(ofs << 1) + gidx];
  let pidx = bitcast<u32>(dir.w);
  let throughput4 = select(pathStatesIn[gidx], vec4f(1.0), (pidx & 0xff) == 0); // Avoid mem access on bounce 0
  var throughput = throughput4.xyz;

  // For bounce 0 we index into direct col buf, further bounces into indirect col buf
  let cidx = min(pidx & 0xff, 1u) * ofs + (pidx >> 8);

  // No hit, terminate path
  if(hit.x == INF) {
    colBuf[cidx] += vec4f(throughput * config.bgColor.xyz, 1.0);
    return;
  }

  var pos: vec3f;
  var nrm: vec3f;
  var ltriId: u32;
  var mtl: Mtl;
  finalizeHit(ori.xyz, dir.xyz, hit, &pos, &nrm, &ltriId, &mtl);

  // Hit light via material direction
  if(mtl.emissive > 0.0) {
    // Lights emit from front side only
    if(dot(-dir.xyz, nrm) > 0) {
      // Bounces > 0
      if((pidx & 0xff) > 0) {
        // Secondary ray hit light, apply MIS
        let ldir = pos - ori.xyz;
        let ldistInv = inverseSqrt(dot(ldir, ldir));
        let pdf = throughput4.w * geomSolidAngle(ldir * ldistInv, nrm, ldistInv); // throughput4.w = pdf
        let lpdf = sampleLTriUniformPdf(ltriId);
        let weight = pdf / (pdf + lpdf);
        colBuf[cidx] += vec4f(throughput * weight * mtl.col.xyz, 1.0);
      } else {
        // Primary ray hit light
        colBuf[cidx] += vec4f(throughput * mtl.col.xyz, 1.0);
      }
    }
    // Terminate ray after light hit (lights do not reflect)
    return;
  }

  // Increase roughness with each bounce to combat fireflies
  //mtl.roughness = min(1.0, mtl.roughness + f32(pidx & 0xff) * mtl.roughness * 0.75);
  //mtl.roughness = select(1.0, mtl.roughness, (pidx & 0xff) == 0u);

  seed = bitcast<u32>(ori.w);
  let r0 = rand4();

  // Russian roulette
  // Terminate with prob inverse to throughput
  let p = min(0.95, maxComp3(throughput));
  if(r0.w > p) { // (pidx & 0xff) > 0
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
    let bsdf = evalMaterialCombined(mtl, -dir.xyz, nrm, ldir, &fres);
    let bsdfPdf = sampleMaterialCombinedPdf(mtl, -dir.xyz, nrm, ldir, fres);
    let weight = lpdf / (lpdf + bsdfPdf * gsa);
    // Init shadow ray
    let sidx = atomicAdd(&config.shadowRayCnt, 1u);
    shadowRays[             sidx] = vec4f(pos, bitcast<f32>(cidx));
    shadowRays[       ofs + sidx] = vec4f(ldir, 1.0 / ldistInv - 2.0 * EPS);
    shadowRays[(ofs << 1) + sidx] = vec4f(throughput * bsdf * gsa * weight * emission * saturate(dot(nrm, ldir)) / lpdf, 0.0);
  }

  // Reached max bounces (encoded in lower 8 bits of height), terminate path
  if((pidx & 0xff) == (h & 0xff) - 1) {
    return;
  }

  // Sample material
  var wi: vec3f;
  var fres: vec3f;
  var isSpecRefl: bool;
  var pdf: f32;
  if(!sampleMaterial(mtl, -dir.xyz, nrm, r0.xyz, &wi, &fres, &isSpecRefl, &pdf)) {
    return;
  }

  // Apply bsdf
  throughput *= evalMaterial(mtl, -dir.xyz, nrm, wi, fres, isSpecRefl) * abs(dot(nrm, wi)) / pdf;

  // Get compacted index into the path state buffer
  let gidxNext = atomicAdd(&config.extRayCnt, 1u);

  // Init next path segment 
  pathStatesOut[             gidxNext] = vec4f(throughput, pdf);
  pathStatesOut[       ofs + gidxNext] = vec4f(pos, bitcast<f32>(seed));
  pathStatesOut[(ofs << 1) + gidxNext] = vec4f(wi, bitcast<f32>(pidx + 1)); // Keep pixel index but increase bounce num in lowest 8 bits
}
