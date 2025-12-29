# CMakeLists.txt – Root level (modern, no GLOB)
cmake_minimum_required(VERSION 3.25)  # 3.25+ for better target_sources handling
project(CarrotGameEngine LANGUAGES CXX C)  # Need C for the Wayland protocol file

# Global settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories (bin/Debug, bin/Release, etc.)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG Debug Release)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG})
endforeach()

# Find Vulkan (includes glslangValidator)
find_package(Vulkan REQUIRED)

# ====================
# Main executable: Carrot
# ====================
add_executable(Carrot)

# Group sources logically — easy to extend
target_sources(Carrot PRIVATE
        src/Engine/Engine.cpp
        src/Engine/Engine.h
        src/Engine/Module.h
        src/Engine/Core/Application.cpp
        src/Engine/Core/Application.h
        src/Engine/Core/Module.h
        src/Engine/Core/Platform/Wayland/WaylandWindow.cpp
        src/Engine/Core/Platform/Wayland/WaylandWindow.h
        src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.c
        src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.h
        src/Engine/Debug/DebugOverlay.cpp
        src/Engine/Debug/DebugOverlay.h
        src/Engine/HotReload/ShaderWatcher.cpp
        src/Engine/HotReload/ShaderWatcher.h
        src/Engine/Renderer/VulkanRenderer.cpp
        src/Engine/Renderer/VulkanRenderer.h
        src/Engine/RHI/VulkanContext.cpp
        src/Engine/RHI/VulkanContext.h
        src/Engine/Utils/ShaderUtils.cpp
        src/Engine/Utils/ShaderUtils.h
        src/Engine/Window/Window.cpp
        src/Engine/Window/Window.h
        src/Game/main.cpp
)

# Include directories
target_include_directories(Carrot PRIVATE
        src
        deps/glm
        deps/stb
        deps/spdlog/include
        deps/tracy/public
        ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(Carrot PRIVATE Vulkan::Vulkan)

# Platform-specific configuration
if(UNIX AND NOT APPLE)  # Linux
    # Force Clang for both C and C++ to ensure consistent libc++ usage
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    # Apply libc++ only to C++ compilation and linking
    target_compile_options(Carrot PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
    )
    target_link_options(Carrot PRIVATE
            $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
    )

    target_compile_definitions(Carrot PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)

    target_link_libraries(Carrot PRIVATE
            wayland-client
            wayland-cursor
            wayland-egl
            xkbcommon
            dl
            pthread
    )

elseif(APPLE)  # macOS
    target_compile_definitions(Carrot PRIVATE VK_USE_PLATFORM_MACOS_MVK)
    target_link_libraries(Carrot PRIVATE
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
    )

elseif(WIN32)
    target_compile_definitions(Carrot PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

# Debug configuration
target_compile_definitions(Carrot PRIVATE
        $<$<CONFIG:Debug>:DEBUG CARROT_ENABLE_TRACY>
)

# Optimization levels
target_compile_options(Carrot PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O3>
)

# ====================
# Shader compilation
# ====================
file(GLOB SHADER_SOURCES
        "shaders/*.vert"
        "shaders/*.frag"
)

set(SPV_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_OUTPUT ${SPV_OUTPUT_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
            OUTPUT ${SPV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_OUTPUT_DIR}
            COMMAND Vulkan::glslangValidator -V ${SHADER} -o ${SPV_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${SPV_OUTPUT})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPV_OUTPUTS})
add_dependencies(Carrot CompileShaders)

# ====================
# Asset copying
# ====================
add_custom_command(TARGET Carrot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/Engine/Assets
        $<TARGET_FILE_DIR:Carrot>/assets
)