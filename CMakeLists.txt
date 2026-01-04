cmake_minimum_required(VERSION 4.1)
project(CarrotGameEngine LANGUAGES CXX C VERSION 0.0.1)

# ------------------------------------------------------------------------
# Default build type for single-config generators (Ninja, Makefiles)
# ------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ------------------------------------------------------------------------
# Core settings
# ------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories (bin/Debug, bin/Release, etc.)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG Debug Release)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG})
endforeach()

# ------------------------------------------------------------------------
# Engine Sources
# ------------------------------------------------------------------------
set(CARROT_ENGINE_SOURCES
        src/Engine/Engine.cpp
        src/Engine/Engine.h
        src/Engine/Core/Application.cpp
        src/Engine/Core/Application.h
        src/Engine/Core/Platform/Wayland/WaylandWindow.cpp
        src/Engine/Core/Platform/Wayland/WaylandWindow.h
        src/Engine/Debug/DebugOverlay.cpp
        src/Engine/Debug/DebugOverlay.h
        src/Engine/HotReload/ShaderWatcher.cpp
        src/Engine/HotReload/ShaderWatcher.h
        src/Engine/RHI/Backends/Vulkan/VulkanRenderer.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanRenderer.h
        src/Engine/RHI/Backends/Vulkan/VulkanContext.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanContext.h
        src/Engine/Utils/ShaderUtils.cpp
        src/Engine/Utils/ShaderUtils.h
        src/Engine/Window/Window.cpp
        src/Engine/Window/Window.h
        src/Engine/Common/CommonHeaders.h
        src/Engine/Utils/MulticastDelegate.h
        src/Engine/Core/Logger.cpp
        src/Engine/Core/Logger.h
        src/Engine/Utils/DebugBreak.h
        src/Engine/Utils/Assert.h
        src/Engine/Core/LogSink.h
        src/Engine/Core/LogSink.cpp
        src/Engine/CarrotEngine.h
        src/Engine/Renderer/Renderer.h
        src/Engine/RHI/Backends/Vulkan/VulkanCommon.h
        src/Engine/RHI/Backends/Vulkan/VulkanCore.h
        src/Engine/RHI/RHI.h
        src/Engine/RHI/Device.h
)

add_library(CarrotEngine STATIC ${CARROT_ENGINE_SOURCES})

# ------------------------------------------------------------------------
# Debug/Release macros & optimizations
# ------------------------------------------------------------------------
target_compile_definitions(CarrotEngine PUBLIC
        $<$<CONFIG:Debug>:_DEBUG CARROT_ENABLE_TRACY>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_compile_options(CarrotEngine PUBLIC
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:MinSizeRel>:-Os>
)

# ------------------------------------------------------------------------
# Include directories – split user vs. third-party
# ------------------------------------------------------------------------
# project code → full warnings
target_include_directories(CarrotEngine PUBLIC
        src/Engine
)

# Everything under deps/ is third-party → treat as system headers
target_include_directories(CarrotEngine SYSTEM PRIVATE
        deps
        deps/glm
        deps/stb
        deps/spdlog/include
        deps/tracy/public
        ${Vulkan_INCLUDE_DIRS}
        deps/metal-cpp
        deps/metal-cpp-extensions
)

# ------------------------------------------------------------------------
# Warnings & sanitizers
# ------------------------------------------------------------------------
target_compile_options(CarrotEngine PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Werror=return-type -Wshadow -Wno-missing-braces
        >
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive- /wd4201
        >
)

target_compile_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:APPLE>>>: -fsanitize=address,undefined -fno-omit-frame-pointer>
)

target_link_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:APPLE>>>: -fsanitize=address,undefined>
)

# Explicitly turn off sanitizers on Apple (in case any leak through)
target_compile_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<BOOL:APPLE>>: -fno-sanitize=all>
)
target_link_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<BOOL:APPLE>>: -fno-sanitize=all>
)

# ------------------------------------------------------------------------
# macOS placeholders
# ------------------------------------------------------------------------
if(APPLE)
    target_compile_definitions(CarrotEngine PUBLIC __APPLE__)

    target_link_libraries(CarrotEngine PRIVATE
            "-framework Metal"
            "-framework MetalKit"
            "-framework Cocoa"
            "-framework AppKit"
            "-framework QuartzCore"
    )

    # Set source file that will have the defines applied to them, which must also include all metal-cpp headers
    set(METAL_CPP_PRIVATE_IMPL src/set/file/here.cpp)

    # Make this file contain the five PRIVATE_IMPLEMENTATION defines
    set_source_files_properties(${METAL_CPP_PRIVATE_IMPL}
            PROPERTIES COMPILE_DEFINITIONS
            "NS_PRIVATE_IMPLEMENTATION;CA_PRIVATE_IMPLEMENTATION;MTL_PRIVATE_IMPLEMENTATION;MTK_PRIVATE_IMPLEMENTATION;APPKIT_PRIVATE_IMPLEMENTATION"
    )
# ------------------------------------------------------------------------
# Windows placeholders
# ------------------------------------------------------------------------
elseif(WIN32)
    target_link_libraries(CarrotEngine PRIVATE d3d12 dxgi dxguid)
    target_compile_definitions(CarrotEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
# ------------------------------------------------------------------------
# Linux
# ------------------------------------------------------------------------
elseif(UNIX)
    # Force Clang for both C and C++ to ensure consistent libc++ usage
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    # Find Vulkan (includes glslangValidator)
    find_package(Vulkan REQUIRED)
    target_link_libraries(CarrotEngine PRIVATE Vulkan::Vulkan)

    target_sources(CarrotEngine PRIVATE
            src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.c
            src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.h
    )

    # Apply libc++ only to C++ compilation and linking
    target_compile_options(CarrotEngine PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
    )
    target_link_options(CarrotEngine PUBLIC
            $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
    )

    target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)

    target_link_libraries(CarrotEngine PRIVATE
            wayland-client
            wayland-cursor
            wayland-egl
            xkbcommon
            dl
            pthread
    )

endif()

# ------------------------------------------------------------------------
# Sandbox / Game Executable
# ------------------------------------------------------------------------
set(CARROT_SANDBOX_SOURCES
        src/Game/main.cpp
        src/Game/Game.cpp
        src/Game/Game.h
)

add_executable(CarrotSandbox ${CARROT_SANDBOX_SOURCES})  # Rename if you prefer "Game" or "Sandbox"
target_include_directories(CarrotSandbox PRIVATE src/Game)
target_link_libraries(CarrotSandbox PRIVATE CarrotEngine)  # This pulls in everything needed

# ------------------------------------------------------------------------
# Shader compilation
# ------------------------------------------------------------------------
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
add_dependencies(CarrotSandbox CompileShaders)

# ------------------------------------------------------------------------
# Asset copying
# ------------------------------------------------------------------------
add_custom_command(TARGET CarrotSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/Engine/Assets
        $<TARGET_FILE_DIR:CarrotSandbox>/assets
)