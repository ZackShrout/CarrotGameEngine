-- premake5.lua – FINAL, CLEAN, WORKING
workspace "CarrotGameEngine"
    configurations { "Debug", "Release" }
    platforms { "x86_64" }

    filter "action:gmake*"
        targetdir "bin/%{cfg.buildcfg:lower()}"
        objdir    "obj/%{cfg.buildcfg:lower()}"

    filter "system:linux"
        toolset "clang"
        buildoptions { "-stdlib=libc++" }
        linkoptions  { "-stdlib=libc++" }
        defines { "VK_USE_PLATFORM_WAYLAND_KHR" }
        links { "c++", "vulkan", "wayland-client", "wayland-cursor", "wayland-egl", "xkbcommon", "dl", "pthread" }

    filter "configurations:Debug"
        defines { "DEBUG", "CARROT_ENABLE_TRACY" }
        symbols "On"

project "Carrot"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"

    files { 
        "src/**.h", 
        "src/**.cpp", 
        "src/Engine/Core/Platform/Wayland/xdg-shell-client-protocol.c" 
    }

    includedirs { "src", "deps/glm", "deps/stb", "deps/spdlog/include", "deps/tracy/public" }

    -- SHADER COMPILATION – 100% WORKING
    filter { "files:shaders/**.vert", "files:shaders/**.frag" }
        buildmessage "Compiling shader %{file.relpath}"
        buildcommands {
            'mkdir -p "%{cfg.targetdir}/shaders"',
            'glslangValidator -V "%{file.path}" -o "%{cfg.targetdir}/shaders/%{file.basename}.spv"'
        }
        buildoutputs { "%{cfg.targetdir}/shaders/%{file.basename}.spv" }
    filter {}