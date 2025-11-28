-- premake5.lua
-- Carrot Game Engine – Month 0 (November 2025)
-- Pure Wayland + Vulkan C API, zero external runtime dependencies

workspace "CarrotGameEngine"
    configurations { "Debug", "Release" }
    platforms { "x86_64" }

    -- Force lowercase output directories for sanity
    filter "action:gmake*"
        targetdir "bin/%{cfg.buildcfg:lower()}"
        objdir    "obj/%{cfg.buildcfg:lower()}"

    -- Linux-specific settings
    filter "system:linux"
        toolset "clang"
        buildoptions { "-stdlib=libc++" }
        linkoptions  { "-stdlib=libc++" }
        defines { "VK_USE_PLATFORM_WAYLAND_KHR" }

        links {
            "c++",                  -- libc++
            "vulkan",
            "wayland-client",
            "wayland-cursor",
            "wayland-egl",          -- fixes wl_egl_window_* symbols
            "xkbcommon",
            "dl",
            "pthread"
        }

    filter "configurations:Debug"
        defines { "DEBUG", "CARROT_ENABLE_TRACY" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"

project "Carrot"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"

    -- Source files
    files {
        "src/**.h",
        "src/**.cpp"
    }

    -- Include directories
    includedirs {
        "src",
        "deps/glm",
        "deps/stb",
        "deps/spdlog/include",
        "deps/tracy/public"
        -- Vulkan headers come from system (libvulkan-dev)
    }

    -- No vulkan-hpp in the build at all for Month 0
    -- We use pure Vulkan C API → zero version issues, zero bloat