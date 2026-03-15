//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ShaderWatcher.h"

#include <ShaderToolsConfig.h>
#include <chrono>
#include <filesystem>

namespace carrot::hot_reload {
    namespace fs = std::filesystem;

    void shader_watcher_t::recompile_all() noexcept
    {
        const std::string dir{ std::string(CARROT_SOURCE_ROOT) + "/shaders" };

        try
        {
            for (const auto& entry: fs::directory_iterator(dir))
            {
                if (!entry.is_regular_file()) continue;
                std::string name = entry.path().filename().string();
                if (!name.ends_with(".hlsl")) continue;

                try_compile_and_notify(name);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            LOG_CORE_ERROR("Filesystem error while recompiling all shaders: {}", e.what());
        }
    }

    // PRIVATE
    void shader_watcher_t::try_compile_and_notify(const std::string& filename) noexcept
    {
        const std::string src_path{ std::format("{}/shaders/{}", CARROT_SOURCE_ROOT, filename) };

        // Determine shader stage from filename convention
        std::string profile{ "vs_6_7" }; // fallback
        std::string lower{ filename };
        std::ranges::transform(lower, lower.begin(), ::tolower);

        if (lower.ends_with(".frag.hlsl")) profile = "ps_6_7";
        else if (lower.ends_with(".vert.hlsl")) profile = "vs_6_7";
        else if (lower.ends_with(".comp.hlsl")) profile = "cs_6_7";
        // else if (lower.ends_with(".geom.hlsl")) profile = "gs_6_7";
        // else if (lower.ends_with(".mesh.hlsl")) profile = "ms_6_7";
        // else if (lower.ends_with(".task.hlsl")) profile = "as_6_7";
        // etc.

        // Output filename convention — same as in CompileShaders.cmake
        std::string out_name{ filename };
        if (out_name.ends_with(".hlsl")) out_name.resize(out_name.size() - 5);
        out_name += ".spv";

        const std::string out_rel_path{ "shaders/" + out_name }; // what you give callback
        const std::string out_abs_path{ std::format("{}/{}", fs::current_path().string(), out_rel_path) };

        // Make sure output directory exists
        fs::create_directories(fs::path(out_abs_path).parent_path());

        std::string dxc_exe{ CARROT_DXC_EXECUTABLE };
        if (dxc_exe.empty()) dxc_exe = "dxc"; // fallback

        std::string extra_flags;
        if (profile.starts_with("vs_") || profile.starts_with("gs_") || profile.starts_with("ds_") ||
            profile.starts_with("ms_") || profile.starts_with("as_") || profile == "lib_6_x")
        {
            extra_flags = "-fvk-invert-y ";
        }

        // Build dxc command — keep flags in sync with CompileShaders.cmake
        const std::string cmd{ std::format(
            "\"{}\" -spirv -T {} -E main -fvk-use-scalar-layout -Zi -Od -WX "
            "{}" // ← extra_flags inserted here
            "-fspv-target-env=vulkan1.3 \"{}\" -Fo \"{}\"",
            dxc_exe, profile, extra_flags, src_path, out_abs_path
        ) };

        LOG_GRAPHICS_INFO("[HotReload] Compiling: {} → {} ({})", filename, out_name, profile);

        const int result{ std::system(cmd.c_str()) };
        if (result == 0)
        {
            LOG_GRAPHICS_INFO("[HotReload] Success: {}", filename);
            // Tiny debounce — helps when editor writes files in multiple steps
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (_callback) _callback(out_rel_path);
        }
        else
        {
            LOG_GRAPHICS_ERROR("[HotReload] dxc failed (code {}): {}", /*WEXITSTATUS(*/result/*)*/, filename);
        }
    }

    shader_reload_callback_t shader_watcher_t::_callback;
} // namespace carrot::hot_reload
