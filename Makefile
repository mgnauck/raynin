OUT_DIR=output
SRC=$(shell find -L src/ -type f -name '*.c')
OBJ=$(patsubst src/%.c,obj/%.o,$(SRC))
WASM_OUT=intro
SHADER=$(shell find ./ -maxdepth 1 -type f -name '*.wgsl')
SHADER_MIN=$(patsubst ./%.wgsl,$(OUT_DIR)/%.wgsl.min,$(SHADER))
SHADER_EXCLUDES=vm,m,m1,m2,m3
LOADER_JS=main
AUDIO_JS=audio
OUT=index.html

PLAYER_CONFIG=-DPROJECT_DATA -DPLAYER_ONLY -DBUMP_ALLOCATOR -DNO_LOG -DSKIP_RESET_STATE_ON_VOICE_START
#PLAYER_CONFIG=-DPROJECT_DATA -DPLAYER_ONLY -DBUMP_ALLOCATOR -DSKIP_RESET_STATE_ON_VOICE_START
PLAYER_FEATURES = -DSUPPORT_NOTE -DSUPPORT_OSC -DSUPPORT_NOISE2 -DSUPPORT_ADSR -DSUPPORT_TRANSPOSE -DSUPPORT_MUL -DSUPPORT_CLAMP -DSUPPORT_SCALE -DSUPPORT_MIX -DSUPPORT_FILTER -DSUPPORT_DELAY -DSUPPORT_REVERB -DSUPPORT_DIST -DSUPPORT_OUT
CC=clang
LD=wasm-ld
DBGFLAGS=-DNDEBUG
CFLAGS=--target=wasm32 -mbulk-memory -std=c2x -nostdlib -Os -ffast-math -msimd128 -flto -pedantic-errors -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable $(PLAYER_CONFIG) $(PLAYER_FEATURES)
#CFLAGS+=-DSILENT
LDFLAGS=--strip-all --lto-O3 --no-entry --export-dynamic --import-undefined --initial-memory=134217728 -z stack-size=8388608 --Map=$(WASM_OUT).map
WOPTFLAGS=-Oz --enable-bulk-memory --enable-simd

.PHONY: clean

$(OUT_DIR)/$(OUT): $(OUT_DIR)/$(LOADER_JS).5.js Makefile
	js-payload-compress --zopfli-iterations=100 $< $@ 

$(OUT_DIR)/$(LOADER_JS).5.js: $(OUT_DIR)/$(LOADER_JS).4.js
	terser $< -m -c toplevel,passes=5,unsafe=true,pure_getters=true,keep_fargs=false,booleans_as_integers=true --toplevel > $@

$(OUT_DIR)/$(LOADER_JS).4.js: $(OUT_DIR)/$(AUDIO_JS).2.js $(OUT_DIR)/$(LOADER_JS).3.js
	./embed.sh $(OUT_DIR)/$(LOADER_JS).3.js BEGIN_$(AUDIO_JS)_js END_$(AUDIO_JS)_js $(OUT_DIR)/$(AUDIO_JS).2.js $@

$(OUT_DIR)/$(AUDIO_JS).2.js: $(OUT_DIR)/$(AUDIO_JS).1.js
	sed -e 's/\\\\/\\\\/g' -e 's/`/\\`/g' -e 's/$${/\\$${/g' $< > $@

$(OUT_DIR)/$(AUDIO_JS).1.js: $(AUDIO_JS).js
	@mkdir -p `dirname $@`
	terser $< -m -c toplevel,passes=5,unsafe=true,pure_getters=true,keep_fargs=false,booleans_as_integers=false --toplevel > $@

$(OUT_DIR)/$(LOADER_JS).3.js: $(OUT_DIR)/$(WASM_OUT).wasm.deflate.b64 $(SHADER_MIN) $(OUT_DIR)/$(LOADER_JS).2.js
	./embed.sh $(OUT_DIR)/$(LOADER_JS).2.js BEGIN_$(WASM_OUT)_wasm END_$(WASM_OUT)_wasm $(OUT_DIR)/$(WASM_OUT).wasm.deflate.b64 $@

$(OUT_DIR)/%.wgsl.min: ./%.wgsl $(OUT_DIR)/$(LOADER_JS).1.js
	@mkdir -p `dirname $@`
	cp -u $(OUT_DIR)/$(LOADER_JS).1.js $(OUT_DIR)/$(LOADER_JS).2.js
	wgslminify -e $(SHADER_EXCLUDES) $< > $(OUT_DIR)/$(subst .wgsl,.wgsl.min,$<)
	@echo "$(OUT_DIR)/$(subst .wgsl,.wgsl.min,$<):" `wc -c < $(OUT_DIR)/$(subst .wgsl,.wgsl.min,$<)` "bytes"
	./embed.sh $(OUT_DIR)/$(LOADER_JS).2.js BEGIN_$(subst .,_,$<) END_$(subst .,_,$<) $(OUT_DIR)/$(subst .wgsl,.wgsl.min,$<) $(OUT_DIR)/$(LOADER_JS).2.js

$(OUT_DIR)/$(LOADER_JS).1.js: $(LOADER_JS).js $(SHADER)
	@mkdir -p `dirname $@`
	cp $< $(OUT_DIR)/$(LOADER_JS).1.js

$(OUT_DIR)/$(WASM_OUT).wasm.deflate.b64: $(OUT_DIR)/$(WASM_OUT).wasm.deflate
	openssl base64 -A -in $< -out $(OUT_DIR)/$(WASM_OUT).wasm.deflate.b64

$(OUT_DIR)/$(WASM_OUT).wasm.deflate: $(WASM_OUT).wasm
	@mkdir -p `dirname $@`
	js-payload-compress --dump-compressed-raw --write-no-html $< $(OUT_DIR)/$(WASM_OUT).wasm.deflate
	mv $(OUT_DIR)/$(WASM_OUT).wasm.deflate.raw $(OUT_DIR)/$(WASM_OUT).wasm.deflate

$(WASM_OUT).wasm: $(OBJ)
	$(LD) $^ $(LDFLAGS) -o $@
	@echo "$@:" `wc -c < $@` "bytes"
	wasm-opt --legalize-js-interface $(WOPTFLAGS) $@ -o $@
	@echo "$@ (optimized):" `wc -c < $@` "bytes"

obj/%.o: src/%.c
	@mkdir -p `dirname $@`
	$(CC) $(DBGFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj $(OUT_DIR) $(WASM_OUT).wasm $(WASM_OUT).map
