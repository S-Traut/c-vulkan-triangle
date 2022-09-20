shaders: 
	- glslc src/shaders/shader.vert -o src/shaders/vertex.spv
	- glslc src/shaders/shader.frag -o src/shaders/fragment.spv

run: app
	./dist/app

app: shaders
	gcc -g -Isrc/include/ ./src/main.c -lglfw -lvulkan -o ./dist/app