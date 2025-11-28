-- premake5.lua
workspace "CarrotGameEngine"
    configurations { "Debug", "Release" }
    platforms { "x86_64" }

    filter "system:linux"
        toolset "clang"   -- we’ll use clang++ everywhere for consistency
        links { "vulkan", "wayland-client", "wayland-cursor", "xkbcommon" }

project "Carrot"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    targetdir "bin/%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"

    files {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs {
        "deps/glm",
        "deps/vulkan-hpp",
        "deps/stb",
        "deps/spdlog/include",
        "deps/tracy"
    }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"