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

const bufType = { GLOB: 0, MTL: 1, INST: 2, TRI: 3, LTRI: 4, NODE: 5, SRAY: 6, HIT: 7, PATH0: 8, PATH1: 9, ACC: 10, FRAME: 11 };
const pipelineType = { GENERATE: 0, INTERSECT: 1, SHADE: 2, SHADOW: 3, BLIT: 4 };
const bindGroupType = { GENERATE0: 0, GENERATE1: 1, INTERSECT0: 2, INTERSECT1: 3, SHADE0: 4, SHADE1: 5, SHADOW: 6, BLIT: 7 };

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

  res.buf[bufType.GLOB] = device.createBuffer({
    size: globSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.MTL] = device.createBuffer({
    size: mtlSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.INST] = device.createBuffer({
    size: instSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  // No mesh in scene, keep min size buffers, for proper mapping to our layout/shader
  triSz = triSz == 0 ? 96 : triSz;
  ltriSz = ltriSz == 0 ? 80 : ltriSz;
  nodeSz = nodeSz == 0 ? 32 : nodeSz;

  res.buf[bufType.TRI] = device.createBuffer({
    size: triSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.LTRI] = device.createBuffer({
    size: ltriSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.NODE] = device.createBuffer({
    size: nodeSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.SRAY] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.HIT] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[bufType.PATH0] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[bufType.PATH1] = device.createBuffer({
    size: 4 * 4 + WIDTH * HEIGHT * 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[bufType.ACC] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.FRAME] = device.createBuffer({
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

  res.bindGroups[bindGroupType.GENERATE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: 1, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 2, resource: { buffer: res.buf[bufType.PATH0] } },
    ]
  });

  res.bindGroups[bindGroupType.GENERATE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: 1, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 2, resource: { buffer: res.buf[bufType.PATH1] } },
    ]
  });

  res.pipelineLayouts[pipelineType.GENERATE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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

  res.bindGroups[bindGroupType.INTERSECT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 1, resource: { buffer: res.buf[bufType.INST] } },
      { binding: 2, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: 3, resource: { buffer: res.buf[bufType.NODE] } },
      { binding: 4, resource: { buffer: res.buf[bufType.PATH0] } },
      { binding: 5, resource: { buffer: res.buf[bufType.HIT] } },
    ]
  });

  res.bindGroups[bindGroupType.INTERSECT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 1, resource: { buffer: res.buf[bufType.INST] } },
      { binding: 2, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: 3, resource: { buffer: res.buf[bufType.NODE] } },
      { binding: 4, resource: { buffer: res.buf[bufType.PATH1] } },
      { binding: 5, resource: { buffer: res.buf[bufType.HIT] } },
    ]
  });

  res.pipelineLayouts[pipelineType.INTERSECT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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

  res.bindGroups[bindGroupType.SHADE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: 1, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 2, resource: { buffer: res.buf[bufType.MTL] } },
      { binding: 3, resource: { buffer: res.buf[bufType.INST] } },
      { binding: 4, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: 5, resource: { buffer: res.buf[bufType.LTRI] } },
      { binding: 6, resource: { buffer: res.buf[bufType.PATH0] } },
      { binding: 7, resource: { buffer: res.buf[bufType.HIT] } },
      { binding: 8, resource: { buffer: res.buf[bufType.SRAY] } },
      { binding: 9, resource: { buffer: res.buf[bufType.PATH1] } },
      { binding: 10, resource: { buffer: res.buf[bufType.ACC] } },
    ]
  });

  res.bindGroups[bindGroupType.SHADE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: 1, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 2, resource: { buffer: res.buf[bufType.MTL] } },
      { binding: 3, resource: { buffer: res.buf[bufType.INST] } },
      { binding: 4, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: 5, resource: { buffer: res.buf[bufType.LTRI] } },
      { binding: 6, resource: { buffer: res.buf[bufType.PATH1] } },
      { binding: 7, resource: { buffer: res.buf[bufType.HIT] } },
      { binding: 8, resource: { buffer: res.buf[bufType.SRAY] } },
      { binding: 9, resource: { buffer: res.buf[bufType.PATH0] } },
      { binding: 10, resource: { buffer: res.buf[bufType.ACC] } },
    ]
  });

  res.pipelineLayouts[pipelineType.SHADE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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

  res.bindGroups[bindGroupType.SHADOW] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 1, resource: { buffer: res.buf[bufType.INST] } },
      { binding: 2, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: 3, resource: { buffer: res.buf[bufType.NODE] } },
      { binding: 4, resource: { buffer: res.buf[bufType.SRAY] } },
      { binding: 5, resource: { buffer: res.buf[bufType.ACC] } },
    ]
  });

  res.pipelineLayouts[pipelineType.SHADOW] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Blit
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
    ]
  });

  res.bindGroups[bindGroupType.BLIT] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[bufType.FRAME] } },
      { binding: 1, resource: { buffer: res.buf[bufType.ACC] } },
    ]
  });

  res.pipelineLayouts[pipelineType.BLIT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

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
  device.queue.writeBuffer(res.buf[bufType.FRAME], 0, new Uint32Array([WIDTH, HEIGHT, frame, (sample << 8) | (MAX_BOUNCES & 0xff)]));

  // Compute passes
  let gidx = 0;
  for(let i=0; i<SPP; i++) {

    // Initialize buffer count
    device.queue.writeBuffer(res.buf[bufType.PATH0 + gidx], 0, new Uint32Array([WIDTH * HEIGHT, 0, 0, 0]));

    let commandEncoder = device.createCommandEncoder();

    if(sample == 0)
      commandEncoder.clearBuffer(res.buf[bufType.ACC]);

    let passEncoder = commandEncoder.beginComputePass();

    passEncoder.setBindGroup(0, res.bindGroups[bindGroupType.GENERATE0 + gidx]);
    passEncoder.setPipeline(res.pipelines[pipelineType.GENERATE]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    for(let j=0; j<MAX_BOUNCES; j++) {

      if(j > 0)
        passEncoder = commandEncoder.beginComputePass();
      
      passEncoder.setBindGroup(0, res.bindGroups[bindGroupType.INTERSECT0 + gidx]);
      passEncoder.setPipeline(res.pipelines[pipelineType.INTERSECT]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
      
      passEncoder.setBindGroup(0, res.bindGroups[bindGroupType.SHADE0 + gidx]);
      passEncoder.setPipeline(res.pipelines[pipelineType.SHADE]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

      passEncoder.setBindGroup(0, res.bindGroups[bindGroupType.SHADOW]);
      passEncoder.setPipeline(res.pipelines[pipelineType.SHADOW]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
      
      passEncoder.end();

      // Reset sray/path data counts
      commandEncoder.clearBuffer(res.buf[bufType.SRAY], 0, 4);
      commandEncoder.clearBuffer(res.buf[bufType.PATH0 + gidx], 0, 4);
      
      // Switch bind groups to select the other path data buffer
      gidx = 1 - gidx;
    }

    device.queue.submit([commandEncoder.finish()]);

    // Update sample count
    device.queue.writeBuffer(res.buf[bufType.FRAME], 3 * 4, new Uint32Array([(++sample << 8) | (MAX_BOUNCES & 0xff)]));
  }

  // Render passes (blit)
  const commandEncoder = device.createCommandEncoder();

  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
  const passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);
  passEncoder.setBindGroup(0, res.bindGroups[bindGroupType.BLIT]);
  passEncoder.setPipeline(res.pipelines[pipelineType.BLIT]);
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
    createPipeline(pipelineType.GENERATE, await (await fetch("generate.wgsl")).text(), "m");
    createPipeline(pipelineType.INTERSECT, await (await fetch("intersect.wgsl")).text(), "m");
    createPipeline(pipelineType.SHADE, await (await fetch("shade.wgsl")).text(), "m");
    createPipeline(pipelineType.SHADOW, await (await fetch("traceShadowRay.wgsl")).text(), "m");
    createPipeline(pipelineType.BLIT, await (await fetch("blit.wgsl")).text(), "vm", "m");
  } else {
    createPipeline(pipelineType.GENERATE, GENERATE_SHADER, "m");
    createPipeline(pipelineType.INTERSECT, INTERSECT_SHADER, "m");
    createPipeline(pipelineType.SHADE, SHADE_SHADER, "m");
    createPipeline(pipelineType.SHADOW, TRACE_SHADOW_SHADER, "m");
    createPipeline(pipelineType.BLIT, BLIT_SHADER, "vm", "m");
  }

  // Start
  if(FULLSCREEN)
    document.addEventListener("click", startRender, { once: true });
  else
    startRender();
}

main();
