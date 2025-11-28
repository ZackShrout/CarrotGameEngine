mkdir -p bin/debug/shaders
glslangValidator -V shaders/triangle.vert -o bin/debug/shaders/triangle.vert.spv
glslangValidator -V shaders/triangle.frag -o bin/debug/shaders/triangle.frag.spv
glslangValidator -V shaders/debug_overlay.vert -o bin/debug/shaders/debug_overlay.vert.spv
glslangValidator -V shaders/debug_overlay.frag -o bin/debug/shaders/debug_overlay.frag.spv
echo "Shaders recompiled"