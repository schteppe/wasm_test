all:
	em++ -g4 -O0 -s USE_PTHREADS=1 src/main.cpp src/esUtil.c src/esShapes.c src/esTransform.c -Iinclude --shell-file shell_minimal.html -o index.html
	cat index.js | sed 's/ {{MODULE_ADDITIONS}}/# sourceMappingURL=index.wasm.map/g' > tmp.js
	mv tmp.js index.js