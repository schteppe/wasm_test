all:
	em++ src/main.cpp src/esUtil.c src/esShapes.c src/esTransform.c -Iinclude -o index.html
