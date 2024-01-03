const FULLSCREEN = false;
const ASPECT = 16.0 / 10.0;
const CANVAS_WIDTH = 1280;
const CANVAS_HEIGHT = Math.ceil(CANVAS_WIDTH / ASPECT);

const SAMPLES_PER_PIXEL = 5;
const MAX_BOUNCES = 5;
//const TEMPORAL_WEIGHT = 0.1;

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const VISUAL_SHADER = `BEGIN_visual_wgsl
END_visual_wgsl`;

let wa;

let canvas;
let context;
let device;
let globalsBuffer;
let bvhNodesBuffer;
let objectsBuffer;
let shapesBuffer;
let materialsBuffer;
let bindGroup;
let pipelineLayout;
let computePipeline;
let renderPipeline;
let renderPassDescriptor;

//const ACTIVE_SCENE = "SPHERES";
//const ACTIVE_SCENE = "QUADS";
const ACTIVE_SCENE = "RIOW";

const MOVE_VELOCITY = 0.1;
const LOOK_VELOCITY = 0.015;

let startTime;
//let gatheredSamples = 0;

let phi, theta;
let eye, right, up, fwd;
let vertFov, focDist, focAngle;
let pixelDeltaX, pixelDeltaY, pixelTopLeft;

let orbitCam = false;

let rand = xorshift32(471849323);

function loadTextFile(url)
{
  return fetch(url).then(response => response.text());
}

function xorshift32(a)
{
  return function()
  {
    a ^= a << 13;
    a ^= a >>> 17;
    a ^= a << 5;
    return (a >>> 0) / 4294967296;
  }
}

function randRange(min, max)
{
  return min + rand() * (max - min);
}

function vec3Rand()
{
  return[rand(), rand(), rand()];
}

function vec3RandRange(min, max)
{
  return [randRange(min, max), randRange(min, max), randRange(min, max)];
}

function vec3Add(a, b)
{
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function vec3Mul(a, b)
{
  return [a[0] * b[0], a[1] * b[1], a[2] * b[2]];
}

function vec3Negate(v)
{
  return [-v[0], -v[1], -v[2]];
}

function vec3Scale(v, s)
{
  return [v[0] * s, v[1] * s, v[2] * s];
}

function vec3Cross(a, b)
{
  return [
    a[1] * b[2] - a[2] * b[1],
    a[2] * b[0] - a[0] * b[2],
    a[0] * b[1] - a[1] * b[0]];
}

function vec3Normalize(v)
{
  let invLen = 1.0 / Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  return [v[0] * invLen, v[1] * invLen, v[2] * invLen];
}

function vec3Length(v)
{
  return Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

function vec3FromSpherical(theta, phi)
{
  return [
    -Math.cos(phi) * Math.sin(theta),
    -Math.cos(theta),
    Math.sin(phi) * Math.sin(theta)];
}

async function createComputePipeline(shaderModule, pipelineLayout, entryPoint)
{
  return device.createComputePipelineAsync({
    layout: pipelineLayout,
    compute: {
      module: shaderModule,
      entryPoint: entryPoint
    }
  });
}

async function createRenderPipeline(shaderModule, pipelineLayout,
  vertexEntry, fragmentEntry)
{
  return device.createRenderPipelineAsync({
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

function encodeComputePassAndSubmit(commandEncoder, pipeline, bindGroup,
  countX, countY, countZ)
{
  const passEncoder = commandEncoder.beginComputePass();
  passEncoder.setPipeline(pipeline);
  passEncoder.setBindGroup(0, bindGroup);
  passEncoder.dispatchWorkgroups(countX, countY, countZ);
  passEncoder.end();
}

function encodeRenderPassAndSubmit(commandEncoder, pipeline, bindGroup, view)
{
  renderPassDescriptor.colorAttachments[0].view = view;
  const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
  passEncoder.setPipeline(pipeline);
  passEncoder.setBindGroup(0, bindGroup);
  passEncoder.draw(4);
  passEncoder.end();
}

async function createGpuResources(
  globalsBufSize, bvhBufSize, objectBufSize, shapeBufSize, materialBufSize)
{
  globalsBuffer = device.createBuffer({
    size: globalsBufSize,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  bvhNodesBuffer = device.createBuffer({
    size: bvhBufSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  objectsBuffer = device.createBuffer({
    size: objectBufSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  shapesBuffer = device.createBuffer({
    size: shapeBufSize,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  materialsBuffer = device.createBuffer({
    size: materialBufSize,
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

  bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: globalsBuffer } },
      { binding: 1, resource: { buffer: bvhNodesBuffer } },
      { binding: 2, resource: { buffer: objectsBuffer } },
      { binding: 3, resource: { buffer: shapesBuffer } },
      { binding: 4, resource: { buffer: materialsBuffer } },
      { binding: 5, resource: { buffer: accumulationBuffer } },
      { binding: 6, resource: { buffer: imageBuffer } }
    ]
  });

  pipelineLayout = device.createPipelineLayout(
    { bindGroupLayouts: [bindGroupLayout] } );

  renderPassDescriptor =
    { colorAttachments: [
      { undefined, // view
        clearValue: {r: 1.0, g: 0.0, b: 0.0, a: 1.0},
        loadOp: "clear",
        storeOp: "store"
      } ]
    };

  await createPipelines();
}

async function createPipelines()
{
  let shaderCode;
  if(VISUAL_SHADER.includes("END_visual_wgsl"))
    shaderCode = await loadTextFile("visual.wgsl");
  else
    shaderCode = VISUAL_SHADER;

  let shaderModule = device.createShaderModule({code: shaderCode});

  computePipeline = await createComputePipeline(
    shaderModule, pipelineLayout, "computeMain");
  
  renderPipeline = await createRenderPipeline(
    shaderModule, pipelineLayout, "vertexMain", "fragmentMain");
}

let last;

function render(time)
{  
  if(last !== undefined) {
    let frameTime = (performance.now() - last);
    document.title = `${(frameTime).toFixed(2)} / ${(1000.0 / frameTime).toFixed(2)}`;
  }
  last = performance.now();

  if(startTime === undefined)
    startTime = time;

  time = (time - startTime) / 1000.0;
  //setPerformanceTimer();

  wa.update(time);
 
  device.queue.writeBuffer(globalsBuffer, 0, wa.memUint8, wa.get_globals_buf(), wa.get_globals_buf_size());
 
  /*device.queue.writeBuffer(globalsBuffer, 4 * 4, new Float32Array(
    [ rand(),
      SAMPLES_PER_PIXEL / (gatheredSamples + SAMPLES_PER_PIXEL),
      time, 0 ]));*/
  
  let commandEncoder = device.createCommandEncoder();
  encodeComputePassAndSubmit(commandEncoder, computePipeline, bindGroup,
    Math.ceil(CANVAS_WIDTH / 8), Math.ceil(CANVAS_HEIGHT / 8), 1);
  encodeRenderPassAndSubmit(commandEncoder, renderPipeline, bindGroup,
    context.getCurrentTexture().createView());
  device.queue.submit([commandEncoder.finish()]);

  requestAnimationFrame(render);

  //gatheredSamples += SAMPLES_PER_PIXEL;
}

function setPerformanceTimer()
{
  let begin = performance.now();
  device.queue.onSubmittedWorkDone()
    .then(function() {
      let end = performance.now();
      let frameTime = end - begin;
      document.title = `${(frameTime).toFixed(2)} / ${(1000.0 / frameTime).toFixed(2)}`;
    }).catch(function(err) {
      console.log(err);
    });
}
/*
function update(time)
{
  if(orbitCam) {
    let speed = 0.3;
    let radius;
    if(ACTIVE_SCENE == "SPHERES")
      radius = 2;
    if(ACTIVE_SCENE == "QUADS")
      radius = 5;
    if(ACTIVE_SCENE == "RIOW")
      radius = 15;
    let height;
    if(ACTIVE_SCENE == "SPHERES" || ACTIVE_SCENE == "QUADS")
      height = 0.5;
    if(ACTIVE_SCENE == "RIOW")
      height = 2.5;
    setView(
      [ Math.sin(time * speed) * radius, height, Math.cos(time * speed) * radius ],
      vec3Normalize(eye));
  }
}

function updateView()
{
  right = vec3Cross([0, 1, 0], fwd);
  up = vec3Cross(fwd, right);

  let viewportHeight = 2 * Math.tan(0.5 * vertFov * Math.PI / 180) * focDist;
  let viewportWidth = viewportHeight * CANVAS_WIDTH / CANVAS_HEIGHT;

  let viewportRight = vec3Scale(right, viewportWidth);
  let viewportDown = vec3Scale(up, -viewportHeight);

  pixelDeltaX = vec3Scale(viewportRight, 1 / CANVAS_WIDTH);
  pixelDeltaY = vec3Scale(viewportDown, 1 / CANVAS_HEIGHT);

  // viewportTopLeft = global.eye - global.focDist * global.fwd - 0.5 * (viewportRight + viewportDown);
  let viewportTopLeft = vec3Add(eye, 
    vec3Add(
      vec3Negate(vec3Scale(fwd, focDist)), 
      vec3Negate(vec3Scale(vec3Add(viewportRight, viewportDown), 0.5))));

  // pixelTopLeft = viewportTopLeft + 0.5 * (v.pixelDeltaX + v.pixelDeltaY)
  pixelTopLeft = vec3Add(viewportTopLeft, 
    vec3Scale(vec3Add(pixelDeltaX, pixelDeltaY), 0.5)); 

  device.queue.writeBuffer(globalsBuffer, 8 * 4, new Float32Array([
    ...eye, vertFov,
    ...right, focDist,
    ...up, focAngle,
    ...pixelDeltaX, 0,
    ...pixelDeltaY, 0,
    ...pixelTopLeft, 0]));

  // Reset accumulation buffer
  gatheredSamples = TEMPORAL_WEIGHT * SAMPLES_PER_PIXEL;
}

function setView(lookFrom, lookAt)
{
  eye = lookFrom;
  fwd = vec3Normalize(vec3Add(lookFrom, vec3Negate(lookAt)));

  theta = Math.acos(-fwd[1]);
  phi = Math.atan2(-fwd[2], fwd[0]) + Math.PI;

  updateView();
}

function resetView()
{
  if(ACTIVE_SCENE == "SPHERES") {
    vertFov = 60;
    focDist = 3;
    focAngle = 0;
    setView([0, 0, 2], [0, 0, 0]);
  }

  if(ACTIVE_SCENE == "QUADS") {
    vertFov = 80;
    focDist = 3;
    focAngle = 0;
    setView([0, 0, 9], [0, 0, 0]);
  }

  if(ACTIVE_SCENE == "RIOW") {
    vertFov = 20;
    focDist = 10;
    focAngle = 0.6;
    setView([13, 2, 3], [0, 0, 0]);
  }
}

function handleKeyEvent(e)
{
  switch (e.key) {
    case "a":
      eye = vec3Add(eye, vec3Scale(right, -MOVE_VELOCITY));
      break;
    case "d":
      eye = vec3Add(eye, vec3Scale(right, MOVE_VELOCITY));
      break;
    case "w":
      eye = vec3Add(eye, vec3Scale(fwd, -MOVE_VELOCITY));
      break;
    case "s":
      eye = vec3Add(eye, vec3Scale(fwd, MOVE_VELOCITY));
      break;
    case ",":
      focDist = Math.max(focDist - 0.1, 0.1);
      break;
    case ".":
      focDist += 0.1;
      break;
    case "-":
      focAngle = Math.max(focAngle - 0.1, 0);
      break;
    case "+":
      focAngle += 0.1;
      break;
    case "r":
      resetView();
      break;
    case "o":
      orbitCam = !orbitCam;
      break;
    case "l":
      createPipelines();
      console.log("Visual shader reloaded");
      break;
  }

  updateView();
}

function handleCameraMouseMoveEvent(e)
{ 
  theta = Math.min(Math.max(theta + e.movementY * LOOK_VELOCITY, 0.01), 0.99 * Math.PI);
  
  phi = (phi - e.movementX * LOOK_VELOCITY) % (2 * Math.PI);
  phi += (phi < 0) ? 2.0 * Math.PI : 0;

  fwd = vec3FromSpherical(theta, phi);

  updateView();
}
*/

async function startRender()
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

  /*
  canvas.addEventListener("click", async () => {
    if(!document.pointerLockElement)
      await canvas.requestPointerLock(); // {unadjustedMovement: true}
  });

  document.addEventListener("keydown", handleKeyEvent);

  document.addEventListener("pointerlockchange", () => {
    if(document.pointerLockElement === canvas) {
      canvas.addEventListener("mousemove", handleCameraMouseMoveEvent);
    } else {
      canvas.removeEventListener("mousemove", handleCameraMouseMoveEvent);
    }
  });
  */
  
  requestAnimationFrame(render);
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
    get_time: () => performance.now(),
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

  await createGpuResources(wa.get_globals_buf_size(), wa.get_bvh_node_buf_size(), wa.get_obj_buf_size(), wa.get_shape_buf_size(), wa.get_mat_buf_size());
 
  device.queue.writeBuffer(bvhNodesBuffer, 0, wa.memUint8, wa.get_bvh_node_buf(), wa.get_bvh_node_buf_size()); 
  device.queue.writeBuffer(objectsBuffer, 0, wa.memUint8, wa.get_obj_buf(), wa.get_obj_buf_size()); 
  device.queue.writeBuffer(shapesBuffer, 0, wa.memUint8, wa.get_shape_buf(), wa.get_shape_buf_size());
  device.queue.writeBuffer(materialsBuffer, 0, wa.memUint8, wa.get_mat_buf(), wa.get_mat_buf_size());

  //device.queue.writeBuffer(globalsBuffer, 0, wa.memUint8, wa.get_globals_buf(), wa.get_globals_buf_size());
  //device.queue.writeBuffer(globalsBuffer, 0, new Uint32Array([CANVAS_WIDTH, CANVAS_HEIGHT, SAMPLES_PER_PIXEL, MAX_BOUNCES]));

  //resetView();
  //updateView();

  document.body.innerHTML = "<button>CLICK<canvas style='width:0;cursor:none'>";
  canvas = document.querySelector("canvas");
  canvas.width = CANVAS_WIDTH;
  canvas.height = CANVAS_HEIGHT;

  let presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  if(presentationFormat !== "bgra8unorm")
    throw new Error(`Expected canvas pixel format of bgra8unorm but was '${presentationFormat}'.`);

  context = canvas.getContext("webgpu");
  context.configure({device, format: presentationFormat, alphaMode: "opaque"});

  if(FULLSCREEN)
    document.querySelector("button").addEventListener("click", startRender);
  else
    startRender();
}

main();
