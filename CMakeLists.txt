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
        src/Engine/RHI/RHI.cpp
        src/Engine/Core/Module.h
        src/Engine/Core/Platform/Platform.h
        src/Engine/Core/Platform/Window.h
        src/Engine/Events/Events.h
        src/Engine/Input/KeyCodes.h
        src/Engine/Input/PlatformKeyMapping.cpp
        src/Engine/Input/PlatformKeyMapping.h
        src/Engine/Utils/JSON/Private/JsonToken.h
        src/Engine/Utils/JSON/Private/JsonAST.h
        src/Engine/Utils/JSON/Private/JsonLexer.cpp
        src/Engine/Utils/JSON/Private/JsonLexer.h
        src/Engine/Utils/JSON/Private/JsonParser.cpp
        src/Engine/Utils/JSON/Private/JsonParser.h
        src/Engine/Utils/JSON/Private/JsonDocument.cpp
        src/Engine/Utils/JSON/Public/JsonDocument.h
        src/Engine/Utils/File/FileUtils.cpp
        src/Engine/Utils/File/FileUtils.h
        src/Engine/Core/Memory/Arena.cpp
        src/Engine/Core/Memory/Arena.h
        src/Engine/Utils/JSON/Private/JsonObjectView.cpp
        src/Engine/Utils/JSON/Public/JsonObjectView.h
        src/Engine/Utils/JSON/Private/JsonValueView.cpp
        src/Engine/Utils/JSON/Public/JsonValueView.h
        src/Engine/Utils/JSON/Private/JsonArrayView.cpp
        src/Engine/Utils/JSON/Public/JsonArrayView.h
        src/Engine/EngineConfig.cpp
        src/Engine/EngineConfig.h
        src/Engine/Audio/Core/AudioClock.h
        src/Engine/Audio/AudioModule.cpp
        src/Engine/Audio/AudioModule.h
        src/Engine/Audio/Backend/AudioBackend.cpp
        src/Engine/Audio/Backend/AudioBackend.h
        src/Engine/Audio/Core/AudioEngine.cpp
        src/Engine/Audio/Core/AudioEngine.h
        src/Engine/Audio/Backend/Null/NullAudioBackend.cpp
        src/Engine/Audio/Backend/Null/NullAudioBackend.h
        src/Engine/Audio/Core/AudioCommand.h
        src/Engine/Audio/Core/AudioCommandQueue.h
        src/Engine/Audio/Voice/Voice.h
        src/Engine/Audio/Core/AudioCore.h
        src/Engine/Audio/DSP/Envelope.cpp
        src/Engine/Audio/DSP/Envelope.h
        src/Engine/Audio/DSP/Pan.h
        src/Engine/Audio/Mixer/AudioBus.h
        src/Engine/Audio/Mixer/AudioRouting.h
        src/Engine/Audio/Mixer/AudioMixer.cpp
        src/Engine/Audio/Mixer/AudioMixer.h
        src/Engine/Audio/Sample/AudioSample.h
        src/Engine/Audio/Sample/WavLoader.cpp
        src/Engine/Audio/Sample/WavLoader.h
        src/Engine/Audio/Audio.h
        src/Engine/Audio/Audio.cpp
        src/Engine/Audio/AudioTypes.h
        src/Engine/Audio/Core/AudioService.h
        src/Engine/Audio/Core/AudioService.cpp
        src/Engine/Assets/AssetID.h
        src/Engine/Assets/AssetHandle.h
        src/Engine/Assets/AssetRegistry.h
        src/Engine/Assets/AssetDatabase.h
        src/Engine/Assets/Audio/AudioAsset.h
        src/Engine/Assets/Audio/AudioAssetRegistry.cpp
        src/Engine/Assets/Audio/AudioAssetRegistry.h
        src/Engine/Assets/Audio/AudioAssetImporter.cpp
        src/Engine/Assets/Audio/AudioAssetImporter.h
        src/Engine/Assets/AssetService.cpp
        src/Engine/Assets/AssetService.h
        src/Engine/Audio/Voice/VoiceHandle.h
        src/Engine/Audio/Core/AudioEvent.h
        src/Engine/Audio/Core/AudioEventQueue.h
        src/Engine/Audio/Streaming/AudioStream.h
        src/Engine/Audio/Core/AudioRingBuffer.h
        src/Engine/Audio/Streaming/TestStreamDecoder.cpp
        src/Engine/Audio/Streaming/TestStreamDecoder.h
        src/Engine/Audio/Sample/WavCore.h
        src/Engine/Audio/Streaming/WavStreamDecoder.cpp
        src/Engine/Audio/Streaming/WavStreamDecoder.h
        src/Engine/Audio/DSP/Resampler.h
        src/Engine/Audio/DSP/DspUnit.h
        src/Engine/Audio/Mixer/FxChain.h
        src/Engine/Audio/DSP/BiquadFilter.h
        src/Engine/Audio/DSP/Delay.h
        src/Engine/Audio/DSP/SchroederReverb.h
        src/Engine/Audio/DSP/Saturator.h
        src/Engine/Audio/DSP/Limiter.h
        src/Engine/Audio/DSP/Baxandall.h
        src/Engine/Audio/DSP/VcaBusComp.h
        src/Engine/Assets/Image/ImageAsset.h
        src/Engine/Assets/Image/ImageLoadError.h
        src/Engine/Assets/Image/ImageLoadError.cpp
        src/Engine/Assets/Image/ImageAssetImporter.cpp
        src/Engine/Assets/Image/ImageAssetImporter.h
        src/Engine/RHI/Backends/Vulkan/VulkanTexture.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanTexture.h
        src/Engine/Assets/Texture/TextureAsset.cpp
        src/Engine/Assets/Texture/TextureAsset.h
        src/Engine/Assets/Texture/TextureAssetImporter.cpp
        src/Engine/Assets/Texture/TextureAssetImporter.h
        src/Engine/IO/VirtualFileSystem.cpp
        src/Engine/IO/VirtualFileSystem.h
        src/Engine/Core/EnginePaths.h
        src/Engine/Assets/AssetManager.cpp
        src/Engine/Assets/AssetManager.h
)

# ------------------------------------------------------------------------
# Platform / Windowing / RHI specific sources
# ------------------------------------------------------------------------
if (APPLE)
    enable_language(OBJCXX)

    set(CMAKE_OBJCXX_STANDARD 23)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)

    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/HotReload/ShaderWatcherApple.mm

            src/Engine/Core/Platform/Cocoa/CocoaWindow.mm
            src/Engine/Core/Platform/Cocoa/CocoaWindow.h
            src/Engine/Core/Platform/Cocoa/AppDelegate.mm
            src/Engine/Core/Platform/Cocoa/AppDelegate.h
            src/Engine/Core/Platform/Cocoa/CocoaInputUtils.h

            src/Engine/RHI/Backends/Metal/MetalCommon.h
            src/Engine/RHI/Backends/Metal/MetalCore.h
            src/Engine/RHI/Backends/Metal/MetalRHIContext.cpp
            src/Engine/RHI/Backends/Metal/MetalRHIContext.h
            src/Engine/RHI/Backends/Metal/MetalDevice.cpp
            src/Engine/RHI/Backends/Metal/MetalDevice.h
            src/Engine/RHI/Backends/Metal/MetalCommandQueue.cpp
            src/Engine/RHI/Backends/Metal/MetalCommandQueue.h
            src/Engine/RHI/Backends/Metal/MetalLayerBridge.mm
            src/Engine/RHI/Backends/Metal/MetalLayerBridge.h
            src/Engine/RHI/Backends/Metal/MetalSwapchain.cpp
            src/Engine/RHI/Backends/Metal/MetalSwapchain.h
            src/Engine/RHI/Backends/Metal/MetalRenderEncoder.cpp
            src/Engine/RHI/Backends/Metal/MetalRenderEncoder.h

            src/Engine/Audio/Backend/Apple/AppleAudioBackend.cpp
            src/Engine/Audio/Backend/Apple/AppleAudioBackend.h
    )
elseif (WIN32)
    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/HotReload/ShaderWatcherWindows.cpp

            src/Engine/Core/Platform/Win32/Win32Window.cpp
            src/Engine/Core/Platform/Win32/Win32Window.h

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
            src/Engine/RHI/Backends/Vulkan/VulkanCommon.h
            src/Engine/RHI/Backends/Vulkan/VulkanCore.h
            src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.cpp
            src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.h

            src/Engine/RHI/Backends/DirectX12/DirectX12Common.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Core.h
            src/Engine/RHI/Backends/DirectX12/DirectX12RHIContext.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12RHIContext.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Device.h
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandQueue.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandQueue.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Swapchain.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Swapchain.h
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandList.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandList.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Fence.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Fence.h

            src/Engine/Audio/Backend/Windows/WindowsAudioBackend.cpp
            src/Engine/Audio/Backend/Windows/WindowsAudioBackend.h
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

    if (CARROT_PLATFORM_WAYLAND)
        list(APPEND CARROT_ENGINE_SOURCES
                src/Engine/Core/Platform/Wayland/WaylandWindow.cpp
                src/Engine/Core/Platform/Wayland/WaylandWindow.h
                src/Engine/Core/Platform/Wayland/Protocols/xdg-shell-client-protocol.c
                src/Engine/Core/Platform/Wayland/Protocols/xdg-shell-client-protocol.h
                src/Engine/Core/Platform/Wayland/Protocols/xdg-decoration-unstable-v1-protocol.c
                src/Engine/Core/Platform/Wayland/Protocols/xdg-decoration-unstable-v1-client-protocol.h
        )
    endif ()

    if (CARROT_PLATFORM_X11)
        list(APPEND CARROT_ENGINE_SOURCES
                # ... X11 windowing .cpp / .h files here ...
        )
    endif ()

    # All Linux platforms get these
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/HotReload/ShaderWatcherLinux.cpp

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
            src/Engine/RHI/Backends/Vulkan/VulkanCommon.h
            src/Engine/RHI/Backends/Vulkan/VulkanCore.h
            src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.cpp
            src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.h

            src/Engine/Audio/Backend/Linux/LinuxAudioBackend.cpp
            src/Engine/Audio/Backend/Linux/LinuxAudioBackend.h
    )

endif()

# ------------------------------------------------------------------------
# External Dependencies
# ------------------------------------------------------------------------
add_subdirectory(deps/CarrotPNG)

# ------------------------------------------------------------------------
# Engine Static Library
# ------------------------------------------------------------------------
add_library(CarrotEngine STATIC ${CARROT_ENGINE_SOURCES})

target_compile_definitions(CarrotEngine PRIVATE
        CARROT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

target_include_directories(CarrotEngine PRIVATE
        ${CMAKE_BINARY_DIR}/src/Engine/HotReload/Config
)

target_link_libraries(CarrotEngine PRIVATE
        CarrotPNG
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
# project code -> full warnings
target_include_directories(CarrotEngine PUBLIC
        src/Engine
)

# Everything under deps/ is third-party -> treat as system headers
target_include_directories(CarrotEngine SYSTEM PUBLIC
        deps
        deps/CarrotHLM/include
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
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive- /wd4201
        >
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
    target_compile_options(CarrotEngine PRIVATE
            -Wno-padded /wd4820
            -Wno-unique-object-duplication
            -Wno-exit-time-destructors -Wno-global-constructors
            -Wno-switch-enum -Wno-switch-default
            -Wno-nrvo
    )
endif()



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
# macOS
# ------------------------------------------------------------------------
if(APPLE)
    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)

    target_compile_definitions(CarrotEngine PUBLIC __APPLE__)

    target_link_libraries(CarrotEngine PRIVATE
            "-framework Metal"
            "-framework MetalKit"
            "-framework Cocoa"
            "-framework AppKit"
            "-framework QuartzCore"
            "-framework CoreServices"
            "-framework CoreAudio"
            "-framework AudioUnit"
            "-framework AudioToolbox"
    )

    # Set source file that will have the defines applied to them, which must also include all metal-cpp headers
    set(METAL_CPP_PRIVATE_IMPL src/Engine/RHI/Backends/Metal/MetalRHIContext.cpp)

    # Make this file contain the five PRIVATE_IMPLEMENTATION defines
    set_source_files_properties(${METAL_CPP_PRIVATE_IMPL}
            PROPERTIES COMPILE_DEFINITIONS
            "NS_PRIVATE_IMPLEMENTATION;CA_PRIVATE_IMPLEMENTATION;MTL_PRIVATE_IMPLEMENTATION;MTK_PRIVATE_IMPLEMENTATION;APPKIT_PRIVATE_IMPLEMENTATION"
    )
# ------------------------------------------------------------------------
# Windows
# ------------------------------------------------------------------------
elseif(WIN32)
    find_package(Vulkan REQUIRED)
    target_link_libraries(CarrotEngine PRIVATE Vulkan::Vulkan)

    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)

    target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_WIN32_KHR)

    target_link_libraries(CarrotEngine PRIVATE d3d12 dxgi dxguid ole32 avrt uuid)
    target_compile_definitions(CarrotEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
# ------------------------------------------------------------------------
# Linux
# ------------------------------------------------------------------------
elseif(UNIX)
    # Find Vulkan
    find_package(Vulkan REQUIRED)
    find_package(ALSA REQUIRED)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(PULSEAUDIO REQUIRED IMPORTED_TARGET
            libpulse-simple
            libpulse
    )

    # Apply libc++ only to C++ compilation and linking
    target_compile_options(CarrotEngine PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
    )
    target_link_options(CarrotEngine PUBLIC
            $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
    )

    # Apply libc++ to CarrotPNG
    target_compile_options(CarrotPNG PUBLIC -stdlib=libc++)
    target_link_options(CarrotPNG PUBLIC -stdlib=libc++)

    target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)

    target_link_libraries(CarrotEngine PRIVATE
            Vulkan::Vulkan
            ALSA::ALSA
            PkgConfig::PULSEAUDIO
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
target_link_libraries(CarrotSandbox PRIVATE CarrotEngine)

#if(WIN32)
#    # Use Windows GUI subsystem → no console window
#    target_link_options(CarrotSandbox PRIVATE "/SUBSYSTEM:WINDOWS")
#
#    # Optional but often needed with clang-cl to avoid entry-point mismatch warnings
#    # This tells linker to look for main() even in WINDOWS subsystem (CRT shim helps)
#   target_link_options(CarrotSandbox PRIVATE "/ENTRY:mainCRTStartup")
#endif()

# ------------------------------------------------------------------------
# Shader compilation
# ------------------------------------------------------------------------
include(CompileShaders.cmake)

file(GLOB HLSL_SOURCES "shaders/*.vert.hlsl" "shaders/*.frag.hlsl")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders")

if(APPLE)
    foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
        file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
        compile_hlsl_to_metallib(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}")
    endforeach()
else()
    if(WIN32)
        foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
            file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
            compile_hlsl_to_dxil(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}")
        endforeach ()
    endif ()
    foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
        file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
        compile_hlsl_to_spirv(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}")
    endforeach()
endif()

# ------------------------------------------------------------------------
# Output
# ------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Platform summary:")
message(STATUS "  Windows        : ${CARROT_PLATFORM_WIN32}")
message(STATUS "  macOS/Cocoa    : ${CARROT_PLATFORM_COCOA}")
message(STATUS "  Linux/Wayland  : ${CARROT_PLATFORM_WAYLAND}")
message(STATUS "  Linux/X11      : ${CARROT_PLATFORM_X11}")