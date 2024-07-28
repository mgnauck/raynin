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

const VISUAL_SHADER = `BEGIN_visual_wgsl
END_visual_wgsl`;

const bufType = { GLOB: 0, MTL: 1, INST: 2, TRI: 3, LTRI: 4, NODE: 5, RAY: 6, SRAY: 7, PATH: 8, ACC: 9, STATE: 10 };
const computePipelineType = { GENERATE: 0, INTERSECT: 1, SHADE: 2, SHADOW: 3 };
const renderPipelineType = { BLIT: 0, BLIT_CONV: 1 };

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
  res.computePipelines = [];
  res.renderPipelines = [];

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

  res.buf[bufType.RAY] = device.createBuffer({
    size: WIDTH * HEIGHT * 8 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[bufType.SRAY] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[bufType.PATH] = device.createBuffer({
    size: WIDTH * HEIGHT * 16 * 4 * 2, // In and out buffer
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[bufType.ACC] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[bufType.STATE] = device.createBuffer({
    size: /* Counter */ 6 * 4 + /* Hit buffer */ WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  let bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: bufType.GLOB,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: { type: "uniform" } },
      { binding: bufType.MTL,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "uniform" } },
      { binding: bufType.INST,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "uniform" } },
      { binding: bufType.TRI,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.LTRI,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.NODE,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.RAY,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "storage" } },
      { binding: bufType.SRAY,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "storage" } },
      { binding: bufType.PATH,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "storage" } },
      { binding: bufType.ACC,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: { type: "storage" } },
      { binding: bufType.STATE,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT, // TODO Refactor so fragment does not need access
        buffer: { type: "storage" } },
    ]
  });

  res.pipelineLayout = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] } );

  res.bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: bufType.GLOB, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: bufType.MTL, resource: { buffer: res.buf[bufType.MTL] } },
      { binding: bufType.INST, resource: { buffer: res.buf[bufType.INST] } },
      { binding: bufType.TRI, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: bufType.LTRI, resource: { buffer: res.buf[bufType.LTRI] } },
      { binding: bufType.NODE, resource: { buffer: res.buf[bufType.NODE] } },
      { binding: bufType.RAY, resource: { buffer: res.buf[bufType.RAY] } },
      { binding: bufType.SRAY, resource: { buffer: res.buf[bufType.SRAY] } },
      { binding: bufType.PATH, resource: { buffer: res.buf[bufType.PATH] } },
      { binding: bufType.ACC, resource: { buffer: res.buf[bufType.ACC] } },
      { binding: bufType.STATE, resource: { buffer: res.buf[bufType.STATE] } },
    ]
  });

  res.renderPassDescriptor =
    { colorAttachments: [
      { undefined, // view
        clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        loadOp: "clear",
        storeOp: "store"
      } ]
    };
}

function createComputePipeline(pipelineType, pipelineLayout, shaderCode, entryPoint)
{
  res.computePipelines[pipelineType] = device.createComputePipeline({
      layout: pipelineLayout,
      compute: { module: device.createShaderModule({ code: shaderCode }), entryPoint: entryPoint }
    });
}

function createRenderPipeline(pipelineType, pipelineLayout, shaderCode, entryPointVertex, entryPointFragment)
{
  let shaderModule = device.createShaderModule({ code: shaderCode });

  res.renderPipelines[pipelineType] = device.createRenderPipeline({
      layout: pipelineLayout,
      vertex: { module: shaderModule, entryPoint: entryPointVertex },
      fragment: { module: shaderModule, entryPoint: entryPointFragment, targets: [{ format: "bgra8unorm" }] },
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

  // Initialize counter index and values
  let counter = new Uint32Array([frame, sample, WIDTH * HEIGHT, /* other ray cnt */ 0, /* shadow ray cnt */ 0, /* counter index */ 0]);
  device.queue.writeBuffer(res.buf[bufType.STATE], 0, counter);

  // Wavefront loop for render passes (compute)
  let counterIndex = 0;
  for(let i=0; i<SPP; i++) {

    let commandEncoder = device.createCommandEncoder();
    let passEncoder = commandEncoder.beginComputePass();
    passEncoder.setBindGroup(0, res.bindGroup);
    passEncoder.setPipeline(res.computePipelines[computePipelineType.GENERATE]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 8), Math.ceil(HEIGHT / 8), 1);
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);

    for(let j=0; j<MAX_BOUNCES; j++) {

      commandEncoder = device.createCommandEncoder();
      passEncoder = commandEncoder.beginComputePass();
      passEncoder.setBindGroup(0, res.bindGroup);
      passEncoder.setPipeline(res.computePipelines[computePipelineType.INTERSECT]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 8), Math.ceil(HEIGHT / 8), 1);
      passEncoder.setPipeline(res.computePipelines[computePipelineType.SHADE]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 8), Math.ceil(HEIGHT / 8), 1);
      passEncoder.setPipeline(res.computePipelines[computePipelineType.SHADOW]);
      passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 8), Math.ceil(HEIGHT / 8), 1);
      passEncoder.end();
      device.queue.submit([commandEncoder.finish()]);

      // Reset current counter to zero, we will start using the other one now
      if(j < MAX_BOUNCES - 1)
        device.queue.writeBuffer(res.buf[bufType.STATE], (2 + (counterIndex & 0x1)) * 4, new Uint32Array([0]));

      // Switch between two counters and associated buffers (rays + path data)
      counterIndex = 1 - counterIndex;

      // Reset shadow ray counter and update toggled counter index
      device.queue.writeBuffer(res.buf[bufType.STATE], 4 * 4, new Uint32Array([0, (counterIndex & 0x1)]));
    }

    // Update sample num and reset counter values
    counter[0] = ++sample;
    counter[1 + (counterIndex & 0x1)] = WIDTH * HEIGHT;
    counter[1 + 1 - (counterIndex & 0x1)] = 0;
    device.queue.writeBuffer(res.buf[bufType.STATE], 1 * 4, counter, 0, 3);
  }

  // Blit pass (fragment)
  const commandEncoder = device.createCommandEncoder();

  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
  const passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);
  passEncoder.setBindGroup(0, res.bindGroup);
  passEncoder.setPipeline(res.renderPipelines[renderPipelineType.BLIT]);
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
  
  // Pipelines
  if(VISUAL_SHADER.includes("END_visual_wgsl")) {

    createComputePipeline(computePipelineType.GENERATE, res.pipelineLayout, await (await fetch("generate.wgsl")).text(), "generate");
    createComputePipeline(computePipelineType.INTERSECT, res.pipelineLayout, await (await fetch("intersect.wgsl")).text(), "intersect");
    createComputePipeline(computePipelineType.SHADE, res.pipelineLayout, await (await fetch("shade.wgsl")).text(), "shade");
    createComputePipeline(computePipelineType.SHADOW, res.pipelineLayout, await (await fetch("traceShadowRay.wgsl")).text(), "traceShadowRay");

    let shaderCode = await (await fetch("blit.wgsl")).text();
    createRenderPipeline(renderPipelineType.BLIT, res.pipelineLayout, shaderCode, "quad", "blit");
    createRenderPipeline(renderPipelineType.BLIT_CONV, res.pipelineLayout, shaderCode, "quad", "blitConverge");

  } else
    createPipelines(VISUAL_SHADER);

  // Start
  if(FULLSCREEN)
    document.addEventListener("click", startRender, { once: true });
  else
    startRender();
}

main();
