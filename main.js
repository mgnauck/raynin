const GLTF_SCENE_PATH = "scenes/scene.gltf";
const GLTF_BIN_PATH = "scenes/scene.bin";

const FULLSCREEN = false;
const ASPECT = 16.0 / 10.0;

const WIDTH = 1280;
const HEIGHT = Math.ceil(WIDTH / ASPECT);

const SPP = 2;
const MAX_BOUNCES = 5;
const CONVERGE = false;

const CAM_LOOK_VELOCITY = 0.005;
const CAM_MOVE_VELOCITY = 0.1;

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const GENERATE_SHADER = `BEGIN_generate_wgsl
END_generate_wgsl`;

const INTERSECT_SHADER = `BEGIN_intersect_wgsl
END_intersect_wgsl`;

const SHADE_SHADER = `BEGIN_shade_wgsl
END_shade_wgsl`;

const TRACE_SHADOW_SHADER = `BEGIN_traceShadowRay_wgsl
END_traceShadowRay_wgsl`;

const BLIT_SHADER = `BEGIN_blit_wgsl
END_blit_wgsl`;

const BUF_GLOB        = 0;
const BUF_MTL         = 1;
const BUF_INST        = 2;
const BUF_TRI         = 3;
const BUF_LTRI        = 4;
const BUF_NODE        = 5;
const BUF_SRAY        = 6;
const BUF_HIT         = 7;
const BUF_PATH0       = 8;
const BUF_PATH1       = 9;
const BUF_ACC         = 10;
const BUF_FRAME       = 11;

const PL_GENERATE     = 0;
const PL_INTERSECT    = 1;
const PL_SHADE        = 2;
const PL_SHADOW       = 3;
const PL_BLIT         = 4;

const BG_GENERATE0    = 0;
const BG_GENERATE1    = 1;
const BG_INTERSECT0   = 2;
const BG_INTERSECT1   = 3;
const BG_SHADE0       = 4;
const BG_SHADE1       = 5;
const BG_SHADOW       = 6;
const BG_BLIT         = 7;

let canvas, context, device;
let wa, res = {};
let frame = 0;
let sample = 0;

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
    gpu_create_res: (g, m, i, t, lt, bn) => createGpuResources(g, m, i, t, lt, bn),
    gpu_write_buf: (id, ofs, addr, sz) => device.queue.writeBuffer(res.buf[id], ofs, wa.memUint8, addr, sz),
    reset_samples: () => { sample = 0; }
  };

  this.instantiate = async function()
  {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
  }
}

function createGpuResources(globSz, mtlSz, instSz, triSz, ltriSz, nodeSz)
{
  res.buf = [];
  res.bindGroups = [];
  res.pipelineLayouts = [];
  res.pipelines = [];

  res.buf[BUF_GLOB] = device.createBuffer({
    size: globSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_MTL] = device.createBuffer({
    size: mtlSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_INST] = device.createBuffer({
    size: instSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  // No mesh in scene, keep min size buffers, for proper mapping to our layout/shader
  triSz = triSz == 0 ? 96 : triSz;
  ltriSz = ltriSz == 0 ? 80 : ltriSz;
  nodeSz = nodeSz == 0 ? 32 : nodeSz;

  res.buf[BUF_TRI] = device.createBuffer({
    size: triSz,
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

  res.buf[BUF_SRAY] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_HIT] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_PATH0] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_PATH1] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_ACC] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_FRAME] = device.createBuffer({
    size: 4 * 4,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  // Generate
  let bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_GENERATE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_GLOB] } },
      { binding: 1, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 2, resource: { buffer: res.buf[BUF_PATH0] } },
    ]
  });

  res.bindGroups[BG_GENERATE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_GLOB] } },
      { binding: 1, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 2, resource: { buffer: res.buf[BUF_PATH1] } },
    ]
  });

  res.pipelineLayouts[PL_GENERATE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Intersect
  bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_INTERSECT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 3, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 4, resource: { buffer: res.buf[BUF_PATH0] } },
      { binding: 5, resource: { buffer: res.buf[BUF_HIT] } },
    ]
  });

  res.bindGroups[BG_INTERSECT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 3, resource: { buffer: res.buf[BUF_NODE] } },
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
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 8, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 9, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 10, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_GLOB] } },
      { binding: 1, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 2, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 3, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 4, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 5, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 6, resource: { buffer: res.buf[BUF_PATH0] } },
      { binding: 7, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_PATH1] } },
      { binding: 10, resource: { buffer: res.buf[BUF_ACC] } },
    ]
  });

  res.bindGroups[BG_SHADE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_GLOB] } },
      { binding: 1, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 2, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 3, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 4, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 5, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 6, resource: { buffer: res.buf[BUF_PATH1] } },
      { binding: 7, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_PATH0] } },
      { binding: 10, resource: { buffer: res.buf[BUF_ACC] } },
    ]
  });

  res.pipelineLayouts[PL_SHADE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Shadow
  bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADOW] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 3, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 4, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 5, resource: { buffer: res.buf[BUF_ACC] } },
    ]
  });

  res.pipelineLayouts[PL_SHADOW] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Blit
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
    ]
  });

  res.bindGroups[BG_BLIT] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_FRAME] } },
      { binding: 1, resource: { buffer: res.buf[BUF_ACC] } },
    ]
  });

  res.pipelineLayouts[PL_BLIT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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

let start = undefined;
let last = undefined;
let frameTimeAvg = undefined;
let updateDisplay = true;

function render(time)
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

  // Initialize frame data
  device.queue.writeBuffer(res.buf[BUF_FRAME], 0, new Uint32Array([WIDTH, HEIGHT, frame, (sample << 8) | (MAX_BOUNCES & 0xff)]));

  // Compute passes
  let gidx = 0;
  for(let i=0; i<SPP; i++) {

    // Initialize buffer count
    device.queue.writeBuffer(res.buf[BUF_PATH0 + gidx], 0, new Uint32Array([WIDTH * HEIGHT]));

    let commandEncoder = device.createCommandEncoder();

    if(sample == 0)
      commandEncoder.clearBuffer(res.buf[BUF_ACC]);

    let passEncoder = commandEncoder.beginComputePass();

    passEncoder.setBindGroup(0, res.bindGroups[BG_GENERATE0 + gidx]);
    passEncoder.setPipeline(res.pipelines[PL_GENERATE]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    for(let j=0; j<MAX_BOUNCES; j++) {

      if(j > 0)
        passEncoder = commandEncoder.beginComputePass();
      
      passEncoder.setBindGroup(0, res.bindGroups[BG_INTERSECT0 + gidx]);
      passEncoder.setPipeline(res.pipelines[PL_INTERSECT]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
      
      passEncoder.setBindGroup(0, res.bindGroups[BG_SHADE0 + gidx]);
      passEncoder.setPipeline(res.pipelines[PL_SHADE]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

      passEncoder.setBindGroup(0, res.bindGroups[BG_SHADOW]);
      passEncoder.setPipeline(res.pipelines[PL_SHADOW]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
      
      passEncoder.end();

      // Reset sray/path data counts
      commandEncoder.clearBuffer(res.buf[BUF_SRAY], 0, 4);
      commandEncoder.clearBuffer(res.buf[BUF_PATH0 + gidx], 0, 4);
      
      // Switch bind groups to select the other path data buffer
      gidx = 1 - gidx;
    }

    device.queue.submit([commandEncoder.finish()]);

    // Update sample count
    device.queue.writeBuffer(res.buf[BUF_FRAME], 3 * 4, new Uint32Array([(++sample << 8) | (MAX_BOUNCES & 0xff)]));
  }

  // Render passes (blit)
  const commandEncoder = device.createCommandEncoder();

  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
  const passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);
  passEncoder.setBindGroup(0, res.bindGroups[BG_BLIT]);
  passEncoder.setPipeline(res.pipelines[PL_BLIT]);
  passEncoder.draw(4);
  passEncoder.end();

  device.queue.submit([commandEncoder.finish()]);

  // Update scene and renderer for next frame
  wa.update((time - start) / 1000, SPP, CONVERGE);
  frame++;

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

  const gpuAdapter = await navigator.gpu.requestAdapter();
  if(!gpuAdapter) {
    alert("Can not use WebGPU. No GPU adapter available.");
    return;
  }

  device = await gpuAdapter.requestDevice();
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

  // Init scene from gltf data
  if(wa.init_scene(gltfPtr, gltf.byteLength, gltfBinPtr, gltfBin.byteLength) > 0) {
    alert("Failed to initialize scene");
    return;
  }

  // Init renderer
  if(wa.init(WIDTH, HEIGHT, SPP, MAX_BOUNCES) > 0) {
    alert("Failed to initialize render resources");
    return;
  }
  
  // Shader modules and pipelines
  if(BLIT_SHADER.includes("END_blit_wgsl")) {
    createPipeline(PL_GENERATE, await (await fetch("generate.wgsl")).text(), "m");
    createPipeline(PL_INTERSECT, await (await fetch("intersect.wgsl")).text(), "m");
    createPipeline(PL_SHADE, await (await fetch("shade.wgsl")).text(), "m");
    createPipeline(PL_SHADOW, await (await fetch("traceShadowRay.wgsl")).text(), "m");
    createPipeline(PL_BLIT, await (await fetch("blit.wgsl")).text(), "vm", "m");
  } else {
    createPipeline(PL_GENERATE, GENERATE_SHADER, "m");
    createPipeline(PL_INTERSECT, INTERSECT_SHADER, "m");
    createPipeline(PL_SHADE, SHADE_SHADER, "m");
    createPipeline(PL_SHADOW, TRACE_SHADOW_SHADER, "m");
    createPipeline(PL_BLIT, BLIT_SHADER, "vm", "m");
  }

  // Start
  if(FULLSCREEN)
    document.addEventListener("click", startRender, { once: true });
  else
    startRender();
}

main();
