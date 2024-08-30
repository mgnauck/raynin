const GLTF_SCENE_PATH = "scenes/scene.gltf";
const GLTF_BIN_PATH = "scenes/scene.bin";

const FULLSCREEN = false;
const ASPECT = 16.0 / 9.0;

const WIDTH = 1280;
const HEIGHT = Math.ceil(WIDTH / ASPECT);

const SPP = 1;
const MAX_BOUNCES = 5;

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
const BUF_NRM         = 11;
const BUF_POS         = 12;
const BUF_DCOL        = 13; // Color buffer direct illumination
const BUF_ICOL        = 14; // Color buffer indirect illumination
const BUF_ACC0        = 15; // Temporal accumulation buffer
const BUF_ACC1        = 16; // Temporal accumulation buffer
const BUF_CFG         = 17; // Accessed from WASM
const BUF_LCAM        = 18;
const BUF_GRID        = 19;

const PL_GENERATE     =  0;
const PL_INTERSECT    =  1;
const PL_SHADE        =  2;
const PL_SHADOW       =  3;
const PL_CONTROL0     =  4;
const PL_CONTROL1     =  5;
const PL_CONTROL2     =  6;
const PL_DENOISE      =  7;
const PL_BLIT         =  8;

const BG_GENERATE     =  0;
const BG_INTERSECT0   =  1;
const BG_INTERSECT1   =  2;
const BG_SHADE0       =  3;
const BG_SHADE1       =  4;
const BG_SHADE2       =  5;
const BG_SHADOW0      =  6;
const BG_SHADOW1      =  7;
const BG_CONTROL      =  8;
const BG_DENOISE0     =  9;
const BG_DENOISE1     = 10;
const BG_BLIT0        = 11;
const BG_BLIT1        = 12;

const WG_SIZE_X       = 16;
const WG_SIZE_Y       = 16;

let canvas, context, device;
let wa, res = {};
let frames = 0;
let samples = 0;
let converge = false;

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
    log_buf: (addr, len) => {
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
    toggle_converge: () => { converge = !converge; }
  };

  this.instantiate = async function()
  {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
  }
}

function createGpuResources(camSz, mtlSz, instSz, triSz, nrmSz, ltriSz, nodeSz)
{
  res.buf = [];
  res.bindGroups = [];
  res.pipelineLayouts = [];
  res.pipelines = [];
  res.accumIdx = 0;

  // Buffers

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

  res.buf[BUF_NRM] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_POS] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_DCOL] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_ICOL] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_ACC0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE //| GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_ACC1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE //| GPUBufferUsage.COPY_DST
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
      { binding: 11, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
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
      { binding: 9, resource: { buffer: res.buf[BUF_NRM] } },
      { binding: 10, resource: { buffer: res.buf[BUF_POS] } },
      { binding: 11, resource: { buffer: res.buf[BUF_DCOL] } },
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
      { binding: 5, resource: { buffer: res.buf[BUF_PATH0] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 7, resource: { buffer: res.buf[BUF_PATH1] } }, // out
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_NRM] } },
      { binding: 10, resource: { buffer: res.buf[BUF_POS] } },
      { binding: 11, resource: { buffer: res.buf[BUF_ICOL] } },
    ]
  });

  res.bindGroups[BG_SHADE2] = device.createBindGroup({
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
      { binding: 9, resource: { buffer: res.buf[BUF_NRM] } },
      { binding: 10, resource: { buffer: res.buf[BUF_POS] } },
      { binding: 11, resource: { buffer: res.buf[BUF_ICOL] } },
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

  res.bindGroups[BG_SHADOW0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 5, resource: { buffer: res.buf[BUF_DCOL] } },
    ]
  });

  res.bindGroups[BG_SHADOW1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 5, resource: { buffer: res.buf[BUF_ICOL] } },
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

  // Denoise
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_DENOISE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_POS] } },
      { binding: 4, resource: { buffer: res.buf[BUF_DCOL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_ICOL] } },
      { binding: 6, resource: { buffer: res.buf[BUF_ACC0] } }, // in
      { binding: 7, resource: { buffer: res.buf[BUF_ACC1] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_POS] } },
      { binding: 4, resource: { buffer: res.buf[BUF_DCOL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_ICOL] } },
      { binding: 6, resource: { buffer: res.buf[BUF_ACC1] } }, // in
      { binding: 7, resource: { buffer: res.buf[BUF_ACC0] } }, // out
    ]
  });

  res.pipelineLayouts[PL_DENOISE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Blit
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
    ]
  });

  res.bindGroups[BG_BLIT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 1, resource: { buffer: res.buf[BUF_ACC0] } },
    ]
  });

  res.bindGroups[BG_BLIT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 1, resource: { buffer: res.buf[BUF_ACC1] } },
    ]
  });

  res.pipelineLayouts[PL_BLIT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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
    passEncoder.setBindGroup(0, res.bindGroups[(j == 0) ? BG_SHADE0 : (BG_SHADE1 + bindGroupPathState)]);
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
    passEncoder.setBindGroup(0, res.bindGroups[(j == 0) ? BG_SHADOW0 : BG_SHADOW1]);
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

  if(updateDisplay) {
    document.title = `${frameTimeAvg.toFixed(2)} / ${(1000.0 / frameTimeAvg).toFixed(2)}`;
    updateDisplay = false;
    setTimeout(() => { updateDisplay = true; }, 100);
  }

  // Initialize config data
  device.queue.writeBuffer(res.buf[BUF_CFG], 0,
    new Uint32Array([
      // Frame data
      WIDTH, (HEIGHT << 8) | (MAX_BOUNCES & 0xff), frames, samples,
      // Path state grid dimensions + w = path cnt
      Math.ceil(WIDTH / WG_SIZE_X), Math.ceil(HEIGHT / WG_SIZE_Y), 1, WIDTH * HEIGHT,
      // Shadow ray buffer grid dimensions + w = shadow ray cnt
      0, 0, 0, 0 ]));
      // Background color + w = ext ray cnt (is not written here, but belongs to the same buffer)

  let commandEncoder = device.createCommandEncoder();

  // Reset accumulated direct and indirect illumination
  if(samples == 0) {
    commandEncoder.clearBuffer(res.buf[BUF_DCOL]);
    commandEncoder.clearBuffer(res.buf[BUF_ICOL]);
  }

  // Accumulate samples
  for(let i=0; i<SPP; i++) {
    accumulateSample(commandEncoder);
    samples++;
  }

  // Temporal accumulation and denoise
  let passEncoder = commandEncoder.beginComputePass();

  passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE0 + res.accumIdx]);
  passEncoder.setPipeline(res.pipelines[PL_DENOISE]);
  passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

  passEncoder.end();

  // Set current cam as last cam
  commandEncoder.copyBufferToBuffer(res.buf[BUF_CAM], 0, res.buf[BUF_LCAM], 0, 12 * 4);

  // Toggle accumulation buffer
  res.accumIdx = 1 - res.accumIdx;

  // Blit to canvas
  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
  
  passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);
  
  passEncoder.setBindGroup(0, res.bindGroups[BG_BLIT0 + res.accumIdx]);
  passEncoder.setPipeline(res.pipelines[PL_BLIT]);
  passEncoder.draw(4);
  
  passEncoder.end();

  device.queue.submit([commandEncoder.finish()]);

  // Update scene and renderer for next frame
  wa.update((time - start) / 1000, converge);
  frames++;

  requestAnimationFrame(render);
}

function startRender()
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

  installEventHandler();
  requestAnimationFrame(render);
}

async function main()
{
  // WebGPU
  if(!navigator.gpu) {
    alert("WebGPU is not supported on this browser.");
    return;
  }

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
  let module = WASM.includes("END_") ?
    await (await fetch("intro.wasm")).arrayBuffer() :
    Uint8Array.from(atob(WASM), (m) => m.codePointAt(0))

  wa = new Wasm(module);
  await wa.instantiate();

  // Create buffers with scene gltf text and binary data
  let gltf = await (await fetch(GLTF_SCENE_PATH)).arrayBuffer();
  let gltfPtr = wa.malloc(gltf.byteLength);
  wa.memUint8.set(new Uint8Array(gltf), gltfPtr);

  let gltfBin = await (await fetch(GLTF_BIN_PATH)).arrayBuffer();
  let gltfBinPtr = wa.malloc(gltfBin.byteLength);
  wa.memUint8.set(new Uint8Array(gltfBin), gltfBinPtr);

  // Init scene from gltf data and alloc GPU resources
  if(wa.init(gltfPtr, gltf.byteLength, gltfBinPtr, gltfBin.byteLength) > 0) {
    alert("Failed to initialize scene");
    return;
  }

  // Shader modules and pipelines
  if(SM_BLIT.includes("END_blit_wgsl")) {
    createPipeline(PL_GENERATE, await (await fetch("generate.wgsl")).text(), "m");
    createPipeline(PL_INTERSECT, await (await fetch("intersect.wgsl")).text(), "m");
    createPipeline(PL_SHADE, await (await fetch("shade.wgsl")).text(), "m");
    createPipeline(PL_SHADOW, await (await fetch("traceShadowRay.wgsl")).text(), "m");
    createPipeline(PL_CONTROL0, await (await fetch("control.wgsl")).text(), "m0");
    createPipeline(PL_CONTROL1, await (await fetch("control.wgsl")).text(), "m1");
    createPipeline(PL_CONTROL2, await (await fetch("control.wgsl")).text(), "m2");
    createPipeline(PL_DENOISE, await (await fetch("denoise.wgsl")).text(), "m");
    createPipeline(PL_BLIT, await (await fetch("blit.wgsl")).text(), "vm", "m");
  } else {
    createPipeline(PL_GENERATE, SM_GENERATE, "m");
    createPipeline(PL_INTERSECT, SM_INTERSECT, "m");
    createPipeline(PL_SHADE, SM_SHADE, "m");
    createPipeline(PL_SHADOW, SM_SHADOW, "m");
    createPipeline(PL_CONTROL0, SM_CONTROL, "m0");
    createPipeline(PL_CONTROL1, SM_CONTROL, "m1");
    createPipeline(PL_CONTROL2, SM_CONTROL, "m2");
    createPipeline(PL_DENOISE, SM_DENOISE, "m");
    createPipeline(PL_BLIT, SM_BLIT, "vm", "m");
  }

  // Start
  if(FULLSCREEN)
    document.addEventListener("click", startRender, { once: true });
  else
    startRender();
}

main();
