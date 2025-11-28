mkdir -p bin/debug/shaders
glslangValidator -V shaders/triangle.vert -o bin/debug/shaders/triangle.vert.spv
glslangValidator -V shaders/triangle.frag -o bin/debug/shaders/triangle.frag.spv
echo "Shaders recompiled"