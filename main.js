const FULLSCREEN = false;
const ASPECT = 16.0 / 10.0;
const CANVAS_WIDTH = 1280;
const CANVAS_HEIGHT = Math.ceil(CANVAS_WIDTH / ASPECT);
const SAMPLES_PER_PIXEL = 5;
const MAX_BOUNCES = 5;

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const VISUAL_SHADER = `BEGIN_visual_wgsl
END_visual_wgsl`;

let canvas, context, device;
let res = {};
let wa;
let start, last;

function handleKeyEvent(e)
{
  // JS key codes do not map well to UTF-8. Only A-Z and 0-9 will work here. Convert A-Z to lower case.
  wa.key_down((e.keyCode >= 65 && e.keyCode <= 90) ? e.keyCode + 32 : e.keyCode);
}

function handleMouseMoveEvent(e)
{
  wa.mouse_move(e.movementX, e.movementY);
}

function installEventHandler()
{
  canvas.addEventListener("click", async () => {
    if(!document.pointerLockElement)
      await canvas.requestPointerLock(); // {unadjustedMovement: true}
  });

  document.addEventListener("keydown", handleKeyEvent);

  document.addEventListener("pointerlockchange", () => {
    if(document.pointerLockElement === canvas) {
      canvas.addEventListener("mousemove", handleMouseMoveEvent);
    } else {
      canvas.removeEventListener("mousemove", handleMouseMoveEvent);
    }
  });
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
      targets: [{format: "bgra8unorm"}]
    },
    primitive: {topology: "triangle-strip"}
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

function createGpuResources(globalsSize, bvhSize, objsSize, shapesSize, materialsSize)
{
  res.globalsBuffer = device.createBuffer({
    size: globalsSize,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.bvhNodesBuffer = device.createBuffer({
    size: bvhSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.objectsBuffer = device.createBuffer({
    size: objsSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.shapesBuffer = device.createBuffer({
    size: shapesSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.materialsBuffer = device.createBuffer({
    size: materialsSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  let accumulationBuffer = device.createBuffer({
    size: CANVAS_WIDTH * CANVAS_HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  let imageBuffer = device.createBuffer({
    size: CANVAS_WIDTH * CANVAS_HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  let bindGroupLayout = device.createBindGroupLayout({
    entries: [ 
      { binding: 0, 
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: {type: "uniform"} },
      { binding: 1, 
        visibility: GPUShaderStage.COMPUTE,
        buffer: {type: "read-only-storage"} },
      { binding: 2,
        visibility: GPUShaderStage.COMPUTE,
        buffer: {type: "read-only-storage"} },
      { binding: 3,
        visibility: GPUShaderStage.COMPUTE,
        buffer: {type: "read-only-storage"}},
      { binding: 4,
        visibility: GPUShaderStage.COMPUTE,
        buffer: {type: "read-only-storage"}},
      { binding: 5,
        visibility: GPUShaderStage.COMPUTE,
        buffer: {type: "storage"}},
      { binding: 6,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        buffer: {type: "storage"}}
    ]
  });

  res.bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.globalsBuffer } },
      { binding: 1, resource: { buffer: res.bvhNodesBuffer } },
      { binding: 2, resource: { buffer: res.objectsBuffer } },
      { binding: 3, resource: { buffer: res.shapesBuffer } },
      { binding: 4, resource: { buffer: res.materialsBuffer } },
      { binding: 5, resource: { buffer: accumulationBuffer } },
      { binding: 6, resource: { buffer: imageBuffer } }
    ]
  });

  res.pipelineLayout = device.createPipelineLayout(
    { bindGroupLayouts: [bindGroupLayout] } );

  res.renderPassDescriptor =
    { colorAttachments: [
      { undefined, // view
        clearValue: {r: 1.0, g: 0.0, b: 0.0, a: 1.0},
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
 
  device.queue.writeBuffer(res.globalsBuffer, 0, wa.memUint8, wa.globals_buf(), wa.globals_buf_size());
 
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

  document.querySelector("button").removeEventListener("click", startRender);

  installEventHandler();

  start = last = performance.now();
  render();
}

function Wasm(module)
{
  this.environment = {
    console_log_buf: (addr, len) => {
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
    powf: (b, e) => Math.pow(b, e)
  };

  this.instantiate = async function()
  {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
    console.log(`Available memory in wasm module: ${(this.memory.buffer.byteLength / (1024 * 1024)).toFixed(2)} MiB`);
  }
}

async function main()
{
  if(!navigator.gpu)
    throw new Error("WebGPU is not supported on this browser.");

  const gpuAdapter = await navigator.gpu.requestAdapter();
  if(!gpuAdapter)
    throw new Error("Can not use WebGPU. No GPU adapter available.");

  device = await gpuAdapter.requestDevice();
  if(!device)
    throw new Error("Failed to request logical device.");

  let module = WASM.includes("END_") ?
    await (await fetch("intro.wasm")).arrayBuffer() :
    Uint8Array.from(atob(WASM), (m) => m.codePointAt(0))

  wa = new Wasm(module);
  await wa.instantiate();
  
  wa.init(CANVAS_WIDTH, CANVAS_HEIGHT, SAMPLES_PER_PIXEL, MAX_BOUNCES);

  createGpuResources(wa.globals_buf_size(), wa.bvh_buf_size(), wa.obj_buf_size(), wa.shape_buf_size(), wa.mat_buf_size());
  
  if(VISUAL_SHADER.includes("END_visual_wgsl"))
    createPipelines(await (await fetch("visual.wgsl")).text());
  else
    createPipelines(VISUAL_SHADER);

  device.queue.writeBuffer(res.bvhNodesBuffer, 0, wa.memUint8, wa.bvh_buf(), wa.bvh_buf_size()); 
  device.queue.writeBuffer(res.objectsBuffer, 0, wa.memUint8, wa.obj_buf(), wa.obj_buf_size()); 
  device.queue.writeBuffer(res.shapesBuffer, 0, wa.memUint8, wa.shape_buf(), wa.shape_buf_size());
  device.queue.writeBuffer(res.materialsBuffer, 0, wa.memUint8, wa.mat_buf(), wa.mat_buf_size());

  document.body.innerHTML = "<button>CLICK<canvas style='width:0;cursor:none'>";
  canvas = document.querySelector("canvas");
  canvas.width = CANVAS_WIDTH;
  canvas.height = CANVAS_HEIGHT;

  let presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  if(presentationFormat !== "bgra8unorm")
    throw new Error(`Expected canvas pixel format of bgra8unorm but was '${presentationFormat}'.`);

  context = canvas.getContext("webgpu");
  context.configure({ device, format: presentationFormat, alphaMode: "opaque" });

  if(FULLSCREEN)
    document.querySelector("button").addEventListener("click", startRender);
  else
    startRender();
}

main();
