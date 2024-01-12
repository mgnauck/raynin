OUTDIR=output
SRC=main.c sutil.c mutil.c printf.c log.c vec3.c cfg.c aabb.c scn.c bvh.c shape.c cam.c view.c
OBJ=$(patsubst %.c,obj/%.o,$(SRC))
WASM_OUT=intro
SHADER=visual.wgsl
SHADER_EXCLUDES=computeMain,vertexMain,fragmentMain
LOADER_JS=main
OUT=index.html

CC=clang
LD=wasm-ld
DBGFLAGS=-DNDEBUG
CFLAGS=--target=wasm32 -mbulk-memory -std=c2x -nostdlib -Os -ffast-math -flto -pedantic-errors -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable
#CFLAGS+=-DSILENT
LDFLAGS=--strip-all --lto-O3 --no-entry --export-dynamic --import-undefined --initial-memory=67108864 -z stack-size=8388608
WOPTFLAGS=-Oz --enable-bulk-memory

.PHONY: clean

$(OUTDIR)/$(OUT): $(OUTDIR)/$(LOADER_JS).3.js
	js-payload-compress --zopfli-iterations=100 $< $@ 

$(OUTDIR)/$(LOADER_JS).3.js: $(OUTDIR)/$(LOADER_JS).2.js
	terser $< -m -c toplevel,passes=5,unsafe=true,pure_getters=true,keep_fargs=false,booleans_as_integers=true --toplevel > $@

$(OUTDIR)/$(LOADER_JS).2.js: $(OUTDIR)/$(subst .,.min.,$(SHADER)) $(OUTDIR)/$(LOADER_JS).1.js
	./embed.sh $(OUTDIR)/$(LOADER_JS).1.js BEGIN_$(subst $(OUTDIR)/,,$(subst .min.,_,$<)) END_$(subst $(OUTDIR)/,,$(subst .min.,_,$<)) $< $(OUTDIR)/$(LOADER_JS).2.js

$(OUTDIR)/%.min.wgsl: ./%.wgsl
	@mkdir -p `dirname $@`
	wgslminify -e $(SHADER_EXCLUDES) $< > $(OUTDIR)/$(subst .,.min.,$<)
	@echo "$(OUTDIR)/$(subst .,.min.,$<):" `wc -c < $(OUTDIR)/$(subst .,.min.,$<)` "bytes"

$(OUTDIR)/$(LOADER_JS).1.js: $(OUTDIR)/$(WASM_OUT).base64.wasm $(LOADER_JS).js
	./embed.sh $(LOADER_JS).js BEGIN_$(WASM_OUT)_wasm END_$(WASM_OUT)_wasm $(OUTDIR)/$(WASM_OUT).base64.wasm $@

$(OUTDIR)/$(WASM_OUT).base64.wasm: $(WASM_OUT).wasm
	@mkdir -p `dirname $@`
	openssl base64 -A -in $< -out $(OUTDIR)/$(WASM_OUT).base64.wasm

$(WASM_OUT).wasm: $(OBJ)
	$(LD) $^ $(LDFLAGS) -o $@
	@echo "$@:" `wc -c < $@` "bytes"
	wasm-opt --legalize-js-interface $(WOPTFLAGS) $@ -o $@
	@echo "$@ (optimized):" `wc -c < $@` "bytes"

obj/%.o: src/%.c
	@mkdir -p `dirname $@`
	$(CC) $(DBGFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj $(OUTDIR) $(WASM_OUT).wasm
