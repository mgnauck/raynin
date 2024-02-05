const FULLSCREEN = false;
const ASPECT = 16.0 / 10.0;
const CANVAS_WIDTH = 1280;
const CANVAS_HEIGHT = Math.ceil(CANVAS_WIDTH / ASPECT);

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const VISUAL_SHADER = `BEGIN_visual_wgsl
END_visual_wgsl`;

const bufType = {
  GLOB: 0, TRI: 1, TRI_DATA: 2, INDEX: 3, BVH_NODE: 4,
  TLAS_NODE: 5, INST: 6, MAT: 7, ACC: 8
  //, IMG: 9
};

let canvas, context, device;
let wa, res = {};
let start, last;

function handleMouseMoveEvent(e)
{
  wa.mouse_move(e.movementX, e.movementY);
}

function installEventHandler()
{
  canvas.addEventListener("click", async () => {
    if(!document.pointerLockElement)
      await canvas.requestPointerLock(); // { unadjustedMovement: true }
  });

  document.addEventListener("keydown",
    // Key codes do not map well to UTF-8. Use A-Z and 0-9 only. Convert A-Z to lower case.
    e => wa.key_down((e.keyCode >= 65 && e.keyCode <= 90) ? e.keyCode + 32 : e.keyCode));

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
    time: () => performance.now(),
    sqrtf: (v) => Math.sqrt(v),
    sinf: (v) => Math.sin(v),
    cosf: (v) => Math.cos(v),
    tanf: (v) => Math.tan(v),
    acosf: (v) => Math.acos(v),
    atan2f: (y, x) => Math.atan2(y, x),
    powf: (b, e) => Math.pow(b, e),
    gpu_create_res: (g, t, td, idx, bn, tn, i, m) => createGpuResources(g, t, td, idx, bn, tn, i, m),
    gpu_write_buf: (id, ofs, addr, sz) => device.queue.writeBuffer(res.buf[id], ofs, wa.memUint8, addr, sz)
  };

  this.instantiate = async function()
  {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
  }
}

function createComputePipeline(shaderModule, pipelineLayout, entryPoint)
{
  return device.createComputePipeline({
    layout: pipelineLayout,
    compute: {
      module: shaderModule,
      entryPoint: entryPoint
    }
  });
}

function createRenderPipeline(shaderModule, pipelineLayout, vertexEntry, fragmentEntry)
{
  return device.createRenderPipeline({
    layout: pipelineLayout,
    vertex: {
      module: shaderModule,
      entryPoint: vertexEntry
    },
    fragment: {
      module: shaderModule,
      entryPoint: fragmentEntry,
      targets: [{ format: "bgra8unorm" }]
    },
    primitive: { topology: "triangle-strip" }
  });
}

function encodeComputePassAndSubmit(commandEncoder, pipeline, bindGroup, countX, countY, countZ)
{
  const passEncoder = commandEncoder.beginComputePass();
  passEncoder.setPipeline(pipeline);
  passEncoder.setBindGroup(0, bindGroup);
  passEncoder.dispatchWorkgroups(countX, countY, countZ);
  passEncoder.end();
}

function encodeRenderPassAndSubmit(commandEncoder, pipeline, bindGroup, renderPassDescriptor, view)
{
  renderPassDescriptor.colorAttachments[0].view = view;
  const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
  passEncoder.setPipeline(pipeline);
  passEncoder.setBindGroup(0, bindGroup);
  passEncoder.draw(4);
  passEncoder.end();
}

function createGpuResources(globSz, triSz, triDataSz, indexSz, bvhNodeSz, tlasNodeSz, instSz, matSz)
{
  res.buf = [];

  res.buf[bufType.GLOB] = device.createBuffer({
    size: globSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.TRI] = device.createBuffer({
    size: triSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.TRI_DATA] = device.createBuffer({
    size: triDataSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.INDEX] = device.createBuffer({
    size: indexSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.BVH_NODE] = device.createBuffer({
    size: bvhNodeSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.TLAS_NODE] = device.createBuffer({
    size: tlasNodeSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.INST] = device.createBuffer({
    size: instSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.MAT] = device.createBuffer({
    size: matSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[bufType.ACC] = device.createBuffer({
    size: CANVAS_WIDTH * CANVAS_HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  /*res.buf[bufType.IMG] = device.createBuffer({
    size: CANVAS_WIDTH * CANVAS_HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });*/

  let bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: bufType.GLOB,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: { type: "uniform" } },
      { binding: bufType.TRI,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.TRI_DATA,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.INDEX,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.BVH_NODE,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.TLAS_NODE,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.INST,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.MAT,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "read-only-storage" } },
      { binding: bufType.ACC,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: { type: "storage" } },
      /*{ binding: bufType.IMG,
        visibility: GPUShaderStage.FRAGMENT,
        buffer: { type: "storage" } }*/
    ]
  });

  res.bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: bufType.GLOB, resource: { buffer: res.buf[bufType.GLOB] } },
      { binding: bufType.TRI, resource: { buffer: res.buf[bufType.TRI] } },
      { binding: bufType.TRI_DATA, resource: { buffer: res.buf[bufType.TRI_DATA] } },
      { binding: bufType.INDEX, resource: { buffer: res.buf[bufType.INDEX] } },
      { binding: bufType.BVH_NODE, resource: { buffer: res.buf[bufType.BVH_NODE] } },
      { binding: bufType.TLAS_NODE, resource: { buffer: res.buf[bufType.TLAS_NODE] } },
      { binding: bufType.INST, resource: { buffer: res.buf[bufType.INST] } },
      { binding: bufType.MAT, resource: { buffer: res.buf[bufType.MAT] } },
      { binding: bufType.ACC, resource: { buffer: res.buf[bufType.ACC] } },
      //{ binding: bufType.IMG, resource: { buffer: res.buf[bufType.IMG] } }
    ]
  });

  res.pipelineLayout = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] } );

  res.renderPassDescriptor =
    { colorAttachments: [
      { undefined, // view
        clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        loadOp: "clear",
        storeOp: "store"
      } ]
    };
}

function createPipelines(shaderCode)
{
  let shaderModule = device.createShaderModule({ code: shaderCode });
  res.computePipeline = createComputePipeline(shaderModule, res.pipelineLayout, "computeMain");
  res.renderPipeline = createRenderPipeline(shaderModule, res.pipelineLayout, "vertexMain", "fragmentMain");
}

function render()
{
  let frame = performance.now() - last;
  document.title = `${(frame).toFixed(2)} / ${(1000.0 / frame).toFixed(2)}`;
  last = performance.now();

  wa.update((performance.now() - start) / 1000);
 
  let commandEncoder = device.createCommandEncoder();
  encodeComputePassAndSubmit(commandEncoder, res.computePipeline, res.bindGroup, Math.ceil(CANVAS_WIDTH / 8), Math.ceil(CANVAS_HEIGHT / 8), 1);
  encodeRenderPassAndSubmit(commandEncoder, res.renderPipeline, res.bindGroup, res.renderPassDescriptor, context.getCurrentTexture().createView());
  device.queue.submit([commandEncoder.finish()]);

  requestAnimationFrame(render);
}

function startRender()
{
  if(FULLSCREEN)
    canvas.requestFullscreen();
  else {
    canvas.style.width = CANVAS_WIDTH;
    canvas.style.height = CANVAS_HEIGHT;
    canvas.style.position = "absolute";
    canvas.style.left = 0;
    canvas.style.top = 0;
  }

  installEventHandler();

  start = last = performance.now();
  render();
}

async function main()
{
  // WebGPU
  if(!navigator.gpu)
    throw new Error("WebGPU is not supported on this browser.");

  const gpuAdapter = await navigator.gpu.requestAdapter();
  if(!gpuAdapter)
    throw new Error("Can not use WebGPU. No GPU adapter available.");

  device = await gpuAdapter.requestDevice();
  if(!device)
    throw new Error("Failed to request logical device.");

  // Canvas/context
  document.body.innerHTML = "CLICK<canvas style='width:0;cursor:none'>";
  canvas = document.querySelector("canvas");
  canvas.width = CANVAS_WIDTH;
  canvas.height = CANVAS_HEIGHT;

  let presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  if(presentationFormat !== "bgra8unorm")
    throw new Error(`Expected canvas pixel format of bgra8unorm but was '${presentationFormat}'.`);

  context = canvas.getContext("webgpu");
  context.configure({ device, format: presentationFormat, alphaMode: "opaque" });

  // Load actual code
  let module = WASM.includes("END_") ?
    await (await fetch("intro.wasm")).arrayBuffer() :
    Uint8Array.from(atob(WASM), (m) => m.codePointAt(0))

  wa = new Wasm(module);
  await wa.instantiate();
  
  wa.init(CANVAS_WIDTH, CANVAS_HEIGHT);
  
  // Pipelines
  if(VISUAL_SHADER.includes("END_visual_wgsl"))
    createPipelines(await (await fetch("visual.wgsl")).text());
  else
    createPipelines(VISUAL_SHADER);

  // Start
  if(FULLSCREEN)
    document.addEventListener("click", startRender, { once: true });
  else
    startRender();
}

main();
