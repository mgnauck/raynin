// Sync track
const PLAY_SYNC_TRACK = false;
const BPM = 120;
const ROWS_PER_BEAT = 4;
const TRACK = [
  // Row, event id, value, blend mode (0 = STEP, 1 = LINEAR, 2 = SMOOTH, 3 = RAMP)
   0, 0, 0.0, 0,
   5, 0, 1.0, 0,
  10, 0, 2.0, 0,
  15, 0, 3.0, 0,
  18, 7, 0.0, 1,
  100, 7, 10.0, 1,
];

// Scene loading/export
const LOAD_FROM_GLTF = true;
const PATH_TO_SCENES = "scenes/new/";
const SCENES_TO_LOAD = [
  "good_1.gltf",
  "good_2.gltf",
  "good_3.gltf",
  "good_4.gltf",
  "good_5.gltf",
  "good_6.gltf",
  "good_7.gltf",
  "good_8.gltf",
  "good_9.gltf",
  "good_10.gltf",
];//*/
/*const PATH_TO_SCENES = "scenes/old/";
const SCENES_TO_LOAD = [
  "bernstein.gltf",
  "big-triangle.gltf",
  "donuts.gltf",
  "layers-of-spheres.gltf",
  "only-god-forgives.gltf",
  "purple-motion.gltf",
  "red-skybox.gltf",
  "schellen.gltf",
  "sphere-grid.gltf",
  "triangle-cave.gltf",
  "yellow-blue-hangar.gltf",
  "yellow-donut-cubes.gltf",
];//*/
const EXPORT_BIN_TO_DISK = false && LOAD_FROM_GLTF;
const EXPORT_FILENAME = "scenes-export.bin";
const DO_NOT_LOAD_FROM_JS = false && !LOAD_FROM_GLTF;

const FULLSCREEN = false;
const ASPECT = 16.0 / 9.0;
const WIDTH = 1280;
const HEIGHT = Math.ceil(WIDTH / ASPECT);

const ENABLE_RENDER = true;
const ENABLE_AUDIO = true;

const SPP = 1;
const MAX_BOUNCES = 5; // Max is 15 (encoded in bits 0-3 in frame data)

const CAM_LOOK_VELOCITY = 0.005;
const CAM_MOVE_VELOCITY = 0.1;

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const SM_GENERATE = `BEGIN_generate_wgsl
END_generate_wgsl`;

const SM_INTERSECT = `BEGIN_intersect_wgsl
END_intersect_wgsl`;

const SM_SHADE = `BEGIN_shade_wgsl
END_shade_wgsl`;

const SM_SHADOW = `BEGIN_traceShadowRay_wgsl
END_traceShadowRay_wgsl`;

const SM_CONTROL = `BEGIN_control_wgsl
END_control_wgsl`;

const SM_DENOISE = `BEGIN_denoise_wgsl
END_denoise_wgsl`;

const SM_BLIT = `BEGIN_blit_wgsl
END_blit_wgsl`;

const BUF_CAM         =  0; // Accessed from WASM
const BUF_MTL         =  1; // Accessed from WASM
const BUF_INST        =  2; // Accessed from WASM
const BUF_TRI         =  3; // Accessed from WASM
const BUF_TNRM        =  4; // Accessed from WASM
const BUF_LTRI        =  5; // Accessed from WASM
const BUF_NODE        =  6; // Accessed from WASM
const BUF_PATH0       =  7;
const BUF_PATH1       =  8;
const BUF_SRAY        =  9;
const BUF_HIT         = 10;
const BUF_ATTR        = 11; // Attribute buf (pos and nrm)
const BUF_LATTR       = 12; // Last attribute buf (pos and nrm)
const BUF_COL         = 13; // Separated direct and indirect illumination
const BUF_MOM0        = 14; // Moments buffer (dir/indir)
const BUF_MOM1        = 15; // Moments buffer (dir/indir)
const BUF_HIS0        = 16; // History buffer (moments)
const BUF_HIS1        = 17; // History buffer (moments)
const BUF_ACC0        = 18; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_ACC1        = 19; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_ACC2        = 20; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_CFG         = 21; // Accessed from WASM
const BUF_LCAM        = 22;
const BUF_GRID        = 23;

const PL_GENERATE     =  0;
const PL_INTERSECT    =  1;
const PL_SHADE        =  2;
const PL_SHADOW       =  3;
const PL_CONTROL0     =  4;
const PL_CONTROL1     =  5;
const PL_CONTROL2     =  6;
const PL_CONTROL3     =  7;
const PL_DENOISE0     =  8;
const PL_DENOISE1     =  9;
const PL_DENOISE2     = 10;
const PL_BLIT0        = 12;
const PL_BLIT1        = 13;

const BG_GENERATE     =  0;
const BG_INTERSECT0   =  1;
const BG_INTERSECT1   =  2;
const BG_SHADE0       =  3;
const BG_SHADE1       =  4;
const BG_SHADOW       =  5;
const BG_CONTROL      =  6;
const BG_DENOISE0     =  7;
const BG_DENOISE1     =  8;
const BG_DENOISE2     =  9;
const BG_DENOISE3     = 10;
const BG_DENOISE4     = 11;
const BG_DENOISE5     = 12;
const BG_BLIT0        = 13;
const BG_BLIT1        = 14;
const BG_BLIT2        = 15;

const WG_SIZE_X       = 16;
const WG_SIZE_Y       = 16;

let canvas, context, device;
let wa, res = {};
let frames = 0;
let samples = 0;
let ltriCount = 0;
let converge = false;
let filter = true;
let reproj = filter | false;

function handleMouseMoveEvent(e)
{
  wa.mouse_move(e.movementX, e.movementY, CAM_LOOK_VELOCITY);
}

function installEventHandler()
{
  canvas.addEventListener("click", async () => {
    if(!document.pointerLockElement)
      //await canvas.requestPointerLock({ unadjustedMovement: true });
      await canvas.requestPointerLock();
  });

  document.addEventListener("keydown",
    // Key codes do not map well to UTF-8. Use A-Z and 0-9 only. Convert A-Z to lower case.
    e => wa.key_down((e.keyCode >= 65 && e.keyCode <= 90) ? e.keyCode + 32 : e.keyCode, CAM_MOVE_VELOCITY));

  document.addEventListener("pointerlockchange", () => {
    if(document.pointerLockElement === canvas)
      canvas.addEventListener("mousemove", handleMouseMoveEvent);
    else
      canvas.removeEventListener("mousemove", handleMouseMoveEvent);
  });
}

function Wasm(module)
{
  this.environment = {
    console: (level, addr, len) => {
      let s = "";
      for(let i=0; i<len; i++)
        s += String.fromCharCode(this.memUint8[addr + i]);
      console.log(s);
    },
    sqrtf: (v) => Math.sqrt(v),
    sinf: (v) => Math.sin(v),
    cosf: (v) => Math.cos(v),
    tanf: (v) => Math.tan(v),
    asinf: (v) => Math.asin(v),
    acosf: (v) => Math.acos(v),
    atan2f: (y, x) => Math.atan2(y, x),
    powf: (b, e) => Math.pow(b, e),
    fracf: (v) => v % 1,
    atof: (addr) => {
      let s = "", c, i = 0;
      while((c = String.fromCharCode(this.memUint8[addr + i])) != "\0") {
        s += c;
        i++;
      }
      return parseFloat(s); 
    },
    // Cam, mtl, inst, tri, nrm, ltri, bvh node
    gpu_create_res: (c, m, i, t, n, l, b) => createGpuResources(c, m, i, t, n, l, b),
    gpu_write_buf: (id, ofs, addr, sz) => device.queue.writeBuffer(res.buf[id], ofs, wa.memUint8, addr, sz),
    reset_samples: () => { samples = 0; },
    set_ltri_cnt: (n) => { ltriCount = n; },
    toggle_converge: () => { converge = !converge; },
    toggle_filter: () => { filter = !filter; if(filter) reproj = true; samples = 0; res.accumIdx = 0; },
    toggle_reprojection: () => { reproj = !reproj; if(!reproj) filter = false; samples = 0; res.accumIdx = 0; },
    save_binary: (ptr, size) => {
      const blob = new Blob([wa.memUint8.slice(ptr, ptr + size)], { type: "" });
      const anchor = document.createElement('a');
      anchor.href = URL.createObjectURL(blob);
      anchor.download = EXPORT_FILENAME;
      document.body.appendChild(anchor);
      anchor.click();
      document.body.removeChild(anchor);
      console.log("Downloaded exported binary scenes to " + EXPORT_FILENAME + " with " + size + " bytes");
    }
  };

  this.instantiate = async function()
  {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
  }
}

function deferred() {
  let res, rej;
  let p = new Promise((resolve, reject) => { res = resolve; rej = reject; });
  return { promise: p, resolve: res, reject: rej, signal: async () => res() };
}

function Audio(module) {
  this.module = module;
  this.initEvent = deferred();
  this.startTime = 0;

  this.initialize = async function (sequence) {
    console.log("Audio: Initialize...");

    this.audioContext = new AudioContext;
    //await this.audioContext.resume();

    // add audio processor
    // TODO add embedded handling
    // Load audio worklet script from file
    await this.audioContext.audioWorklet.addModule('audio.js');
  
    // defaults are in:1, out:1, channels:2
    this.audioWorklet = new AudioWorkletNode(this.audioContext, 'a', { outputChannelCount: [2] });
    this.audioWorklet.connect(this.audioContext.destination);

    // event handling
    this.audioWorklet.port.onmessage = async (event) => {
      if ('r' in event.data) {
        // synth is ready
        this.initEvent.signal();
      }
    };

    // send wasm
    this.audioWorklet.port.postMessage({ 'w': this.module, s: sequence });

    // wait initialization to complete
    await this.initEvent.promise;
  }

  this.currentTime = function () {
    return (performance.now() - this.startTime) / 1000;
  }

  this.play = async function () {
    console.log(`Audio: Play.`);

    // send message to start rendering
    await this.audioContext.resume();
    this.audioWorklet.port.postMessage({ p: true });

    // reset timer (TODO timer drift compensation)
    this.startTime = performance.now();
  }
}

function createGpuResources(camSz, mtlSz, instSz, triSz, nrmSz, ltriSz, nodeSz) {
  res.buf = [];
  res.bindGroups = [];
  res.pipelineLayouts = [];
  res.pipelines = [];
  res.accumIdx = 0;

  // Buffers

  // No mesh in scene, keep min size buffers, for proper mapping to our layout/shader
  triSz = triSz == 0 ? 48 : triSz;
  nrmSz = nrmSz == 0 ? 48 : nrmSz;
  ltriSz = ltriSz == 0 ? 64 : ltriSz;
  nodeSz = nodeSz == 0 ? 64 : nodeSz;

  res.buf[BUF_CAM] = device.createBuffer({
    size: camSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_MTL] = device.createBuffer({
    size: mtlSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_INST] = device.createBuffer({
    size: instSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_TRI] = device.createBuffer({
    size: triSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_TNRM] = device.createBuffer({
    size: nrmSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_LTRI] = device.createBuffer({
    size: ltriSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_NODE] = device.createBuffer({
    size: nodeSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_PATH0] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_PATH1] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_SRAY] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIT] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ATTR] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_LATTR] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_COL] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_MOM0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_MOM1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIS0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIS1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC2] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_CFG] = device.createBuffer({
    size: 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_LCAM] = device.createBuffer({
    size: camSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_GRID] = device.createBuffer({
    size: 8 * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.INDIRECT
  });

  // Bind groups and pipelines

  // Generate
  let bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_GENERATE] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_PATH0] } },
    ]
  });

  res.pipelineLayouts[PL_GENERATE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Intersect
  bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_INTERSECT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_PATH0] } },
      { binding: 5, resource: { buffer: res.buf[BUF_HIT] } },
    ]
  });

  res.bindGroups[BG_INTERSECT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_PATH1] } },
      { binding: 5, resource: { buffer: res.buf[BUF_HIT] } },
    ]
  });

  res.pipelineLayouts[PL_INTERSECT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Shade
  bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 8, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 9, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 10, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TNRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 4, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 5, resource: { buffer: res.buf[BUF_PATH0] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 7, resource: { buffer: res.buf[BUF_PATH1] } }, // out
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 10, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.bindGroups[BG_SHADE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TNRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 4, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 5, resource: { buffer: res.buf[BUF_PATH1] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 7, resource: { buffer: res.buf[BUF_PATH0] } }, // out
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 10, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.pipelineLayouts[PL_SHADE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Shadow
  bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADOW] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 5, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.pipelineLayouts[PL_SHADOW] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Control
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_CONTROL] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
    ]
  });

  res.pipelineLayouts[PL_CONTROL0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL2] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL3] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Denoise
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // Mom in
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // Mom out
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // His in
      { binding: 8, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // His out
      { binding: 9, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // Col/Var in
      { binding: 10, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // Col/Var out
    ]
  });

  res.bindGroups[BG_DENOISE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // out
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // in
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // out
      { binding: 9, resource: { buffer: res.buf[BUF_ACC0] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC1] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM1] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_MOM0] } }, // out
      { binding: 7, resource: { buffer: res.buf[BUF_HIS1] } }, // in
      { binding: 8, resource: { buffer: res.buf[BUF_HIS0] } }, // out
      { binding: 9, resource: { buffer: res.buf[BUF_ACC1] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC0] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE2] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC1] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC2] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE3] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC0] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC2] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE4] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC2] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC0] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE5] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC2] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC1] } }, // out
    ]
  });

  res.pipelineLayouts[PL_DENOISE0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_DENOISE1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_DENOISE2] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Blit
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
    ]
  });

  res.bindGroups[BG_BLIT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 1, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ACC0] } },
    ]
  });

  res.bindGroups[BG_BLIT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 1, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ACC1] } },
    ]
  });

  res.bindGroups[BG_BLIT2] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 1, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ACC2] } },
    ]
  });

  res.pipelineLayouts[PL_BLIT0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_BLIT1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Render pass descriptor

  res.renderPassDescriptor =
    { colorAttachments: [
      { undefined, // view
        clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        loadOp: "clear",
        storeOp: "store"
      } ]
    };
}

function createPipeline(pipelineType, shaderCode, entryPoint0, entryPoint1)
{
  let shaderModule = device.createShaderModule({ code: shaderCode });

  if(entryPoint1 === undefined)
    res.pipelines[pipelineType] = device.createComputePipeline({
        layout: res.pipelineLayouts[pipelineType],
        compute: { module: shaderModule, entryPoint: entryPoint0 }
      });
  else
    res.pipelines[pipelineType] = device.createRenderPipeline({
        layout: res.pipelineLayouts[pipelineType],
        vertex: { module: shaderModule, entryPoint: entryPoint0 },
        fragment: { module: shaderModule, entryPoint: entryPoint1, targets: [{ format: "bgra8unorm" }] },
        primitive: { topology: "triangle-strip" }
      });
}

function accumulateSample(commandEncoder)
{
  // Copy path state and shadow ray buffer grid dimensions for indirect dispatch
  commandEncoder.copyBufferToBuffer(res.buf[BUF_CFG], 4 * 4, res.buf[BUF_GRID], 0, 8 * 4);

  let passEncoder = commandEncoder.beginComputePass();

  // Dispatch primary ray generation directly
  passEncoder.setBindGroup(0, res.bindGroups[BG_GENERATE]);
  passEncoder.setPipeline(res.pipelines[PL_GENERATE]);
  passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

  let bindGroupPathState = 0;
  for(let j=0; j<MAX_BOUNCES; j++) {

    // Intersect
    passEncoder.setBindGroup(0, res.bindGroups[BG_INTERSECT0 + bindGroupPathState]);
    passEncoder.setPipeline(res.pipelines[PL_INTERSECT]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 0);

    // Shade
    passEncoder.setBindGroup(0, res.bindGroups[BG_SHADE0 + bindGroupPathState]);
    passEncoder.setPipeline(res.pipelines[PL_SHADE]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 0);

    // Update frame data and workgroup grid dimensions
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL0]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.end();

    // Copy path state and shadow ray buffer grid dimensions for indirect dispatch
    commandEncoder.copyBufferToBuffer(res.buf[BUF_CFG], 4 * 4, res.buf[BUF_GRID], 0, 8 * 4);

    passEncoder = commandEncoder.beginComputePass();

    // Trace shadow rays with offset pointing to grid dim of shadow rays
    passEncoder.setBindGroup(0, res.bindGroups[BG_SHADOW]);
    passEncoder.setPipeline(res.pipelines[PL_SHADOW]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 4 * 4);

    // Reset shadow ray cnt. On last bounce, increase sample cnt and reset path cnt for primary ray gen.
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[(j < MAX_BOUNCES - 1) ? PL_CONTROL1 : PL_CONTROL2]);
    passEncoder.dispatchWorkgroups(1);

    // Toggle path state buffer
    bindGroupPathState = 1 - bindGroupPathState;
  }

  passEncoder.end();
}

function reprojectAndFilter(commandEncoder)
{
  let passEncoder = commandEncoder.beginComputePass();

  // Reprojection and temporal accumulation
  passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE0 + res.accumIdx]);
  passEncoder.setPipeline(res.pipelines[PL_DENOISE0]);
  passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

  if(filter) {

    // Estimate variance
    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE1 - res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE1]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // Edge-avoiding a-trous wavelet transform iterations
    // 0
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE0 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 1
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE2 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 2
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE4 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 3
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE3 - res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    /*
    // 4
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE4 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
    //*/
  }

  passEncoder.end();

  // Remember cam and attribute buffer
  commandEncoder.copyBufferToBuffer(res.buf[BUF_CAM], 0, res.buf[BUF_LCAM], 0, 12 * 4);
  commandEncoder.copyBufferToBuffer(res.buf[BUF_ATTR], 0, res.buf[BUF_LATTR], 0, WIDTH * HEIGHT * 4 * 4 * 2);
}

function blit(commandEncoder)
{
  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
  
  passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);

  if(reproj) {
    //passEncoder.setBindGroup(0, res.bindGroups[filter ? BG_BLIT0 + res.accumIdx : BG_BLIT1 - res.accumIdx]); // 3 or 5 filter iterations
    passEncoder.setBindGroup(0, res.bindGroups[filter ? BG_BLIT2 : BG_BLIT1 - res.accumIdx]); // 2 or 4 filter iterations
    passEncoder.setPipeline(res.pipelines[PL_BLIT0]);
  } else {
    passEncoder.setBindGroup(0, res.bindGroups[BG_BLIT0]);
    passEncoder.setPipeline(res.pipelines[PL_BLIT1]);
  }
  passEncoder.draw(4);
  
  passEncoder.end();
}

let start = undefined;
let last = undefined;
let frameTimeAvg = undefined;
let updateDisplay = true;

async function render(time)
{
  if(start === undefined)
    start = time;
  let frameTime = 0;
  if(last !== undefined)
    frameTime = time - last;
  last = time;

  if(frameTimeAvg === undefined)
    frameTimeAvg = frameTime;
  frameTimeAvg = 0.8 * frameTimeAvg + 0.2 * frameTime;

  /*if(updateDisplay)*/ {
    //document.title = `${frameTimeAvg.toFixed(2)} / ${(1000.0 / frameTimeAvg).toFixed(2)}`;
    document.title = `${frameTime.toFixed(2)} / ${(1000.0 / frameTime).toFixed(2)}`;
    updateDisplay = false;
    //setTimeout(() => { updateDisplay = true; }, 100);
  }

  // Initialize config data
  device.queue.writeBuffer(res.buf[BUF_CFG], 0,
    new Uint32Array([
      // Frame data
      (WIDTH << 16) | (ltriCount & 0xffff), (HEIGHT << 16) | (MAX_BOUNCES & 0xf), frames, samples,
      // Path state grid dimensions + w = path cnt
      Math.ceil(WIDTH / WG_SIZE_X), Math.ceil(HEIGHT / WG_SIZE_Y), 1, WIDTH * HEIGHT,
      // Shadow ray buffer grid dimensions + w = shadow ray cnt
      0, 0, 0, 0 ]));
      // Background color + w = ext ray cnt (is not written here, but belongs to the same buffer)

  let commandEncoder = device.createCommandEncoder();

  // Reset accumulated direct and indirect illumination
  if(samples == 0)
    commandEncoder.clearBuffer(res.buf[BUF_COL]);

  // Pathtrace
  for(let i=0; i<SPP; i++) {
    accumulateSample(commandEncoder);
    samples++;
  }

  // SVGF
  if(reproj || filter)
    reprojectAndFilter(commandEncoder);

  // Blit to canvas
  blit(commandEncoder);

  device.queue.submit([commandEncoder.finish()]);

  // Toggle for next frame
  res.accumIdx = 1 - res.accumIdx;

  // Update scene and renderer for next frame
  wa.update((time - start) / 1000, converge);
  frames++;

  requestAnimationFrame(render);
}

async function startRender()
{
  if(FULLSCREEN)
    canvas.requestFullscreen();
  else {
    canvas.style.width = WIDTH;
    canvas.style.height = HEIGHT;
    canvas.style.position = "absolute";
    canvas.style.left = 0;
    canvas.style.top = 0;
  }

  // Prepare for rendering
  wa.update(0, false);

  if (ENABLE_AUDIO)
    await audio.play();

  if (ENABLE_RENDER) {
    installEventHandler();
    requestAnimationFrame(render);
  }
}

async function loadSceneGltf(gltfPath, prepareForExport)
{
  // Create buffers with scene gltf text and binary data
  let gltf = await (await fetch(gltfPath)).arrayBuffer();
  let gltfPtr = wa.malloc(gltf.byteLength);
  wa.memUint8.set(new Uint8Array(gltf), gltfPtr);

  let gltfBin = await (await fetch(gltfPath.replace(/\.gltf/, ".bin"))).arrayBuffer();
  let gltfBinPtr = wa.malloc(gltfBin.byteLength);
  wa.memUint8.set(new Uint8Array(gltfBin), gltfBinPtr);

  // Load scene from gltf data
  return wa.load_scene_gltf(gltfPtr, gltf.byteLength, gltfBinPtr, gltfBin.byteLength, prepareForExport) == 0;
}

async function loadSceneBin(binPath)
{
  let bin = await (await fetch(binPath)).arrayBuffer();
  let binPtr = wa.malloc(bin.byteLength);
  wa.memUint8.set(new Uint8Array(bin), binPtr);
  return wa.load_scenes_bin(binPtr) == 0;
}

async function main()
{
  // WebGPU
  if(!navigator.gpu) {
    alert("WebGPU is not supported on this browser.");
    return;
  }

  //const adapter = await navigator.gpu.requestAdapter();
  const adapter = await navigator.gpu.requestAdapter({ powerPreference: "low-power" });
  //const adapter = await navigator.gpu.requestAdapter({ powerPreference: "high-performance" });
  if(!adapter) {
    alert("Can not use WebGPU. No GPU adapter available.");
    return;
  }

  device = await adapter.requestDevice({ requiredLimits: {
    maxStorageBuffersPerShaderStage: 10, maxStorageBufferBindingSize: 1073741824 } });
  if(!device) {
    alert("Failed to request logical device.");
    return;
  }

  // Canvas/context
  document.body.innerHTML = "CLICK<canvas style='width:0;cursor:none'>";
  canvas = document.querySelector("canvas");
  canvas.width = WIDTH;
  canvas.height = HEIGHT;

  let presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  if(presentationFormat !== "bgra8unorm") {
    alert(`Expected canvas pixel format of bgra8unorm but was '${presentationFormat}'.`);
    return;
  }

  context = canvas.getContext("webgpu");
  context.configure({ device, format: presentationFormat, alphaMode: "opaque" });

  // Load actual code
  let module;
  if(WASM.includes("END_")) {
    // Load wasm from file
    module = await (await fetch("intro.wasm")).arrayBuffer();
  } else {
    // When embedded, the wasm is deflated and base64 encoded. Undo in reverse.
    module = new Blob([Uint8Array.from(atob(WASM), (m) => m.codePointAt(0))]);
    module = await new Response(module.stream().pipeThrough(new DecompressionStream("deflate-raw"))).arrayBuffer();
  }

  wa = new Wasm(module);
  await wa.instantiate();

  // Load scenes
  if(!DO_NOT_LOAD_FROM_JS) {
    let t0 = performance.now();
    if(LOAD_FROM_GLTF) {
      for(let i=0; i<SCENES_TO_LOAD.length; i++) {
        console.log("Trying to load scene " + PATH_TO_SCENES + SCENES_TO_LOAD[i]);
        if(!await loadSceneGltf(PATH_TO_SCENES + SCENES_TO_LOAD[i], EXPORT_BIN_TO_DISK)) {
          alert("Failed to load scene");
          return;
        }
      }
    } else {
      if(!await loadSceneBin(PATH_TO_SCENES + EXPORT_FILENAME)) {
        alert("Failed to load scenes");
        return;
      }
    }
    console.log("Loaded and generated scenes in " + ((performance.now() - t0) / 1000.0).toFixed(2) + "s");

    // Save exported scene to file via download
    if(EXPORT_BIN_TO_DISK) {
      if(wa.export_scenes() > 0) {
        alert("Failed to make a binary export of the available scenes");
        return;
      }
    }
  }

  // Init gpu resources and prepare scene
  t0 = performance.now();
  wa.init(BPM, ROWS_PER_BEAT, TRACK.length / 4);
  console.log("Initialized data and GPU in " + ((performance.now() - t0) / 1000.0).toFixed(2) + "s");

  // Provide event track to wasm
  if(PLAY_SYNC_TRACK) {
    for(let i=0; i<TRACK.length / 4; i++) {
      let e = i << 2;
      wa.add_event(TRACK[e + 0], TRACK[e + 1], TRACK[e + 2], TRACK[e + 3]);
    }
  }

  // Shader modules and pipelines
  if(SM_BLIT.includes("END_blit_wgsl")) {
    createPipeline(PL_GENERATE, await (await fetch("generate.wgsl")).text(), "m");
    createPipeline(PL_INTERSECT, await (await fetch("intersect.wgsl")).text(), "m");
    createPipeline(PL_SHADE, await (await fetch("shade.wgsl")).text(), "m");
    createPipeline(PL_SHADOW, await (await fetch("traceShadowRay.wgsl")).text(), "m");
    createPipeline(PL_CONTROL0, await (await fetch("control.wgsl")).text(), "m");
    createPipeline(PL_CONTROL1, await (await fetch("control.wgsl")).text(), "m1");
    createPipeline(PL_CONTROL2, await (await fetch("control.wgsl")).text(), "m2");
    createPipeline(PL_CONTROL3, await (await fetch("control.wgsl")).text(), "m3");
    createPipeline(PL_DENOISE0, await (await fetch("denoise.wgsl")).text(), "m");
    createPipeline(PL_DENOISE1, await (await fetch("denoise.wgsl")).text(), "m1");
    createPipeline(PL_DENOISE2, await (await fetch("denoise.wgsl")).text(), "m2");
    createPipeline(PL_BLIT0, await (await fetch("blit.wgsl")).text(), "vm", "m");
    createPipeline(PL_BLIT1, await (await fetch("blit.wgsl")).text(), "vm", "m1");
  } else {
    createPipeline(PL_GENERATE, SM_GENERATE, "m");
    createPipeline(PL_INTERSECT, SM_INTERSECT, "m");
    createPipeline(PL_SHADE, SM_SHADE, "m");
    createPipeline(PL_SHADOW, SM_SHADOW, "m");
    createPipeline(PL_CONTROL0, SM_CONTROL, "m");
    createPipeline(PL_CONTROL1, SM_CONTROL, "m1");
    createPipeline(PL_CONTROL2, SM_CONTROL, "m2");
    createPipeline(PL_CONTROL3, SM_CONTROL, "m3");
    createPipeline(PL_DENOISE0, SM_DENOISE, "m");
    createPipeline(PL_DENOISE1, SM_DENOISE, "m1");
    createPipeline(PL_DENOISE2, SM_DENOISE, "m2");
    createPipeline(PL_BLIT0, SM_BLIT, "vm", "m");
    createPipeline(PL_BLIT1, SM_BLIT, "vm", "m1");
  }

  // Initialize audio
  audio = new Audio(module);
  await audio.initialize(0);

  // Start
  document.addEventListener("click", startRender, { once: true });
}

main();
