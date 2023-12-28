OUTDIR=output
SRC=main.c util.c mathutil.c vec3.c
OBJ=$(patsubst %.c,obj/%.o,$(SRC))
WASM_OUT=intro
SHADER=visual.wgsl
SHADER_EXCLUDES=computeMain,vertexMain,fragmentMain
SHADER_OUT=$(patsubst %.wgsl,$(OUTDIR)/%.min.wgsl,$(SHADER))
LOADER_JS=main
OUT=index.html

CC=clang
LD=wasm-ld
DBGFLAGS=-DNDEBUG
CCFLAGS=--target=wasm32 -std=c2x -pedantic-errors -Wall -Wextra -O3 -flto -nostdlib -Wno-unused-parameter -Wno-unused-variable
LDFLAGS=--strip-all --lto-O3 --no-entry --export-dynamic --import-undefined --initial-memory=67108864 -z stack-size=8388608
WOPTFLAGS=-O3

.PHONY: clean

$(OUTDIR)/$(OUT): $(OUTDIR)/$(LOADER_JS).2.js
	js-payload-compress --zopfli-iterations=100 $< $@ 

$(OUTDIR)/$(LOADER_JS).2.js: $(OUTDIR)/$(LOADER_JS).1.js $(SHADER_OUT)
	@#drop_console=true
	terser $< -m -c toplevel,passes=5,unsafe=true,pure_getters=true,keep_fargs=false,booleans_as_integers=true --toplevel > $@

$(OUTDIR)/%.min.wgsl: %.wgsl
	@mkdir -p `dirname $@`
	wgslminify -e $(SHADER_EXCLUDES) $< > $@
	@echo "$@:" `wc -c < $@` "bytes"
	./embed.sh $(OUTDIR)/$(LOADER_JS).1.js BEGIN_$(subst .,_,$<) END_$(subst .,_,$<) $@ $(OUTDIR)/$(LOADER_JS).1.js

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
	$(CC) $(DBGFLAGS) $(CCFLAGS) -c $< -o $@

clean:
	rm -rf obj $(OUTDIR) $(WASM_OUT).wasm
