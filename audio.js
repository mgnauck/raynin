class AudioProcessor extends AudioWorkletProcessor {
    #engine;
    #initialized;
    #play;
    #bufferLeftPtr;
    #bufferRightPtr;
    #bufferLeft;
    #bufferRight;
    //#renderOffset;
    //#nextTick;
    #startSequence;

    constructor() {
        super();

        this.port.onmessage = async (event) => {
            if ('w' in event.data) {
                //console.log(`Audio[Worklet]: Initializing synth...`);
                //console.log(`Audio[Worklet]: Starting at sequence ${event.data.s}`);

                const importObjects = {
                    env: {
                        // TODO remove me in final version
                        console: (level, addr, len) => {
                            const bytes = new Uint8Array(this.#engine.exports.memory.buffer, addr, len);
                            let s = "";
                            for (let i = 0; i < len; i++)
                                s += String.fromCharCode(bytes[i]);
                            //console.log(s);
                        },
                        sqrtf: (v) => { },
                        // TODO this is a waste of space but probably required :<
                        sinf: (v) => { },
                        cosf: (v) => { },
                        tanf: (v) => { },
                        asinf: (v) => { },
                        acosf: (v) => { },
                        atan2f: (y, x) => { },
                        powf: (b, e) => { },
                        fracf: (v) => { },
                        atof: (addr) => { },
                        gpu_create_res: (c, m, i, t, n, l, b) => { },
                        gpu_write_buf: (id, ofs, addr, sz) => { },
                        reset_samples: () => { },
                        set_ltri_cnt: (n) => { },
                        toggle_converge: () => { },
                        toggle_edit: () => { },
                        toggle_filter: () => { },
                        toggle_reprojection: () => { },
                        save_binary: (ptr, size) => { }
                    }
                }

                this.#engine = await WebAssembly.instantiate(
                    await WebAssembly.compile(event.data.w),
                    importObjects);

                let tunePtr = null;
                if ('t' in event.data) {
                    const tuneData = event.data.t;
                    tunePtr = this.#engine.exports.malloc(tuneData.byteLength);
                    const memory = new Uint8Array(this.#engine.exports.memory.buffer);
                    memory.set(new Uint8Array(tuneData), tunePtr);

                    //console.log(`Audio[Worklet]: Received tune with ${tuneData.byteLength} bytes`);
                }

                // init the project
                this.#startSequence = event.data.s;
                this.#engine.exports.projectInit(sampleRate, this.#startSequence, tunePtr);
            }
            else if ('p' in event.data) {
                this.#play = true;
            }
            else if ('r' in event.data) {
                this.#engine.exports.projectReset(this.#startSequence);
            }
        };
    }

    #initialize(outputs) {
        //console.log("Audio[Worklet]: Initializing buffers...");

        // determine render quantum size
        const renderQuantum = outputs[0][0].length;
        const bufferSize = renderQuantum * 4;

        //console.log(`Audio[Worklet]: Sample rate is ${sampleRate}hz, render quantum is ${renderQuantum}`);

        const memory = this.#engine.exports.memory;

        // allocate stereo buffers (c memory)
        this.#bufferLeftPtr = this.#engine.exports.malloc(bufferSize);
        this.#bufferRightPtr = this.#engine.exports.malloc(bufferSize);

        // allocate stereo buffers (javascript memory)
        this.#bufferLeft = new Float32Array(memory.buffer, this.#bufferLeftPtr, renderQuantum);
        this.#bufferRight = new Float32Array(memory.buffer, this.#bufferRightPtr, renderQuantum);

        //this.#renderOffset = 0;
        //this.#nextTick = 0;

        // send ready message including start time
        this.port.postMessage({ s: this.#engine.exports.audioGetStartTime() });
    }

    process(inputs, outputs) {
        if (this.#engine) {
            !this.#initialized && (this.#initialize(outputs), this.#initialized = true);

            // render next quantum
            if (this.#play) {
                const renderQuantum = outputs[0][0].length;

                this.#engine.exports.audioRender(
                    this.#bufferLeftPtr,
                    this.#bufferRightPtr,
                    renderQuantum);

                // update outputs
                outputs[0][0].set(this.#bufferLeft);
                outputs[0][1].set(this.#bufferRight);

                /*
                // send timer event
                if (this.#renderOffset >= this.#nextTick) {
                    this.port.postMessage({ t: this.#renderOffset / sampleRate });
                    this.#nextTick += sampleRate;
                }

                this.#renderOffset += renderQuantum;
                */
            }
        }

        return true;
    }
}

registerProcessor('a', AudioProcessor);
