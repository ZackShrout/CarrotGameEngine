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
        src/Engine/Debug/DebugOverlay.cpp
        src/Engine/Debug/DebugOverlay.h
        src/Engine/HotReload/ShaderWatcher.cpp
        src/Engine/HotReload/ShaderWatcher.h
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
        src/Engine/Renderer/Renderer.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanCommon.h
        src/Engine/RHI/Backends/Vulkan/VulkanCore.h
        src/Engine/RHI/RHI.h
        src/Engine/RHI/Device.h
        src/Engine/RHI/Swapchain.h
        src/Engine/RHI/CommandQueue.h
        src/Engine/RHI/CommandList.h
        src/Engine/RHI/Buffer.h
        src/Engine/RHI/Texture.h
        src/Engine/RHI/Pipeline.h
        src/Engine/RHI/Fence.h
        src/Engine/RHI/Semaphore.h
        src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.h
        src/Engine/RHI/RHI.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanDevice.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanDevice.h
        src/Engine/RHI/Backends/Vulkan/VulkanSwapchain.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanSwapchain.h
        src/Engine/RHI/Backends/Vulkan/VulkanCommandQueue.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanCommandQueue.h
        src/Engine/RHI/Backends/Vulkan/VulkanRenderPass.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanRenderPass.h
        src/Engine/RHI/Backends/Vulkan/VulkanPipeline.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanPipeline.h
        src/Engine/Core/Module.h
        src/Engine/Core/Platform/Platform.h
        src/Engine/Core/Platform/Window.h
        src/Engine/Events/Events.h
        src/Engine/Input/KeyCodes.h
        src/Engine/Input/PlatformKeyMapping.cpp
        src/Engine/Input/PlatformKeyMapping.h
)

# ------------------------------------------------------------------------
# Platform / Windowing / RHI specific sources
# ------------------------------------------------------------------------
if (APPLE)
    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)
    list(APPEND CARROT_ENGINE_SOURCES
            # ... your Metal-specific .cpp / .h files here ...
    )
elseif (WIN32)
    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)
    list(APPEND CARROT_ENGINE_SOURCES
            # ... your D3D12-specific .cpp / .h files here (if any) ...
    )
elseif (UNIX)
    # Define the cache variable with a proper default value right away.
    # This runs only on first configure (or if deleted from cache).
    # After that, user choices (-D or cmake-gui) are preserved.
    set(CARROT_LINUX_WINDOWING "Auto" CACHE STRING
            "Linux windowing system to target: Auto (prefer Wayland), Wayland, or X11")

    # Restrict the possible values in cmake-gui / ccmake (optional but very nice)
    set_property(CACHE CARROT_LINUX_WINDOWING PROPERTY STRINGS "Auto" "Wayland" "X11")

    # Now resolve which backend we're actually using
    if (CARROT_LINUX_WINDOWING STREQUAL "Auto")
        find_package(PkgConfig QUIET)
        if (PkgConfig_FOUND)
            pkg_check_modules(WL_CLIENT QUIET wayland-client)
            pkg_check_modules(WL_EGL QUIET wayland-egl)
            pkg_check_modules(XKB QUIET xkbcommon)

            if (WL_CLIENT_FOUND AND WL_EGL_FOUND AND XKB_FOUND)
                set(_backend Wayland)
                message(STATUS "Auto: Wayland selected (dev packages detected)")
            else ()
                set(_backend X11)
                message(STATUS "Auto: X11 selected (Wayland dev packages missing)")
            endif ()
        else ()
            set(_backend X11)
            message(STATUS "pkg-config not found → defaulting to X11")
        endif ()
    else ()
        set(_backend ${CARROT_LINUX_WINDOWING})
    endif ()

    # Apply the final choice
    if (_backend STREQUAL "Wayland")
        set(CARROT_PLATFORM_WAYLAND ON CACHE INTERNAL "")
        add_compile_definitions(CARROT_PLATFORM_WAYLAND=1)
    elseif (_backend STREQUAL "X11")
        set(CARROT_PLATFORM_X11 ON CACHE INTERNAL "")
        add_compile_definitions(CARROT_PLATFORM_X11=1)
    else ()
        message(FATAL_ERROR "Internal error: unexpected backend ${_backend}")
    endif ()

    message(STATUS "Linux windowing backend: ${_backend}")

    # Wayland-specific sources (only added when Wayland is chosen)
    if (CARROT_PLATFORM_WAYLAND)
        list(APPEND CARROT_ENGINE_SOURCES
                src/Engine/Core/Platform/Wayland/WaylandWindow.cpp
                src/Engine/Core/Platform/Wayland/WaylandWindow.h
                src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.c
                src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.h
                # ... any other Wayland-only files ...
        )
    endif ()

    # X11-specific sources (only if X11 is active)
    if (CARROT_PLATFORM_X11)
        list(APPEND CARROT_ENGINE_SOURCES
                # ... your X11 windowing .cpp / .h files here ...
        )
    endif ()

    # You can also add Linux-common files here if needed
    # list(APPEND CARROT_ENGINE_SOURCES ...)endif ()

endif()

add_library(CarrotEngine STATIC ${CARROT_ENGINE_SOURCES})

target_compile_definitions(CarrotEngine PRIVATE
        CARROT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

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
# Libraries & Third Party Deps
# ------------------------------------------------------------------------
add_subdirectory(deps/CarrotHLM)
target_link_libraries(CarrotEngine PUBLIC CarrotHLM::CarrotHLM)

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
    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)

    #target_compile_definitions(CarrotEngine PUBLIC __APPLE__)

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
    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)

    target_link_libraries(CarrotEngine PRIVATE d3d12 dxgi dxguid)
    target_compile_definitions(CarrotEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
# ------------------------------------------------------------------------
# Linux
# ------------------------------------------------------------------------
elseif(UNIX)
    # Find Vulkan (includes glslangValidator)
    find_package(Vulkan REQUIRED)
    target_link_libraries(CarrotEngine PRIVATE Vulkan::Vulkan)

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
include(CompileShaders.cmake)

#file(GLOB SHADER_SOURCES "shaders/*.vert" "shaders/*.frag")
#
#set(SPV_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders")
#
#foreach(SHADER_FILE IN LISTS SHADER_SOURCES)
#    file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
#    compile_glsl_to_spv(CarrotSandbox "${REL_SHADER}" "${SPV_OUTPUT_DIR}")
#endforeach()

file(GLOB HLSL_SOURCES "shaders/*.vert.hlsl" "shaders/*.frag.hlsl")

set(SPV_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders")

foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
    file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
    compile_hlsl_to_spirv(CarrotSandbox "${REL_SHADER}" "${SPV_OUTPUT_DIR}")
endforeach()

# ------------------------------------------------------------------------
# Asset copying
# ------------------------------------------------------------------------
add_custom_command(TARGET CarrotSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/Engine/Assets
        $<TARGET_FILE_DIR:CarrotSandbox>/assets
)

# ------------------------------------------------------------------------
# Output
# ------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Platform summary:")
message(STATUS "  Windows        : ${CARROT_PLATFORM_WIN32}")
message(STATUS "  macOS/Cocoa    : ${CARROT_PLATFORM_COCOA}")
message(STATUS "  Linux/Wayland  : ${CARROT_PLATFORM_WAYLAND}")
message(STATUS "  Linux/X11      : ${CARROT_PLATFORM_X11}")