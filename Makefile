OUT_DIR=output
SRC=$(shell find src/ -type f -name '*.c')
OBJ=$(patsubst src/%.c,obj/%.o,$(SRC))
WASM_OUT=intro
SHADER=$(shell find ./ -maxdepth 1 -type f -name '*.wgsl')
SHADER_MIN=$(patsubst ./%.wgsl,$(OUT_DIR)/%.min.wgsl,$(SHADER))
SHADER_EXCLUDES=m,vm,m0,m1,m2
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

$(OUT_DIR)/$(OUT): $(OUT_DIR)/$(LOADER_JS).3.js Makefile
	js-payload-compress --zopfli-iterations=100 $< $@ 

$(OUT_DIR)/$(LOADER_JS).3.js: $(OUT_DIR)/$(LOADER_JS).2.js
	terser $< -m -c toplevel,passes=5,unsafe=true,pure_getters=true,keep_fargs=false,booleans_as_integers=true --toplevel > $@

$(OUT_DIR)/$(LOADER_JS).2.js: $(OUT_DIR)/$(WASM_OUT).base64.wasm $(OUT_DIR)/$(LOADER_JS).1.js $(SHADER_MIN)
	./embed.sh $(OUT_DIR)/$(LOADER_JS).1.js BEGIN_$(WASM_OUT)_wasm END_$(WASM_OUT)_wasm $(OUT_DIR)/$(WASM_OUT).base64.wasm $@

$(OUT_DIR)/%.min.wgsl: ./%.wgsl
	@mkdir -p `dirname $@`
	wgslminify -e $(SHADER_EXCLUDES) $< > $(OUT_DIR)/$(subst .,.min.,$<)
	@echo "$(OUT_DIR)/$(subst .,.min.,$<):" `wc -c < $(OUT_DIR)/$(subst .,.min.,$<)` "bytes"
	./embed.sh $(OUT_DIR)/$(LOADER_JS).1.js BEGIN_$(subst .,_,$<) END_$(subst .,_,$<) $(OUT_DIR)/$(subst .,.min.,$<) $(OUT_DIR)/$(LOADER_JS).1.js

$(OUT_DIR)/$(LOADER_JS).1.js: $(LOADER_JS).js
	@mkdir -p `dirname $@`
	cp $< $(OUT_DIR)/$(LOADER_JS).1.js

$(OUT_DIR)/$(WASM_OUT).base64.wasm: $(WASM_OUT).wasm
	@mkdir -p `dirname $@`
	openssl base64 -A -in $< -out $(OUT_DIR)/$(WASM_OUT).base64.wasm

$(WASM_OUT).wasm: $(OBJ)
	$(LD) $^ $(LDFLAGS) -o $@
	@echo "$@:" `wc -c < $@` "bytes"
	wasm-opt --legalize-js-interface $(WOPTFLAGS) $@ -o $@
	@echo "$@ (optimized):" `wc -c < $@` "bytes"

obj/%.o: src/%.c
	@mkdir -p `dirname $@`
	$(CC) $(DBGFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj $(OUT_DIR) $(WASM_OUT).wasm
