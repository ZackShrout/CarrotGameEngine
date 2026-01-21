//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "ShaderWatcher.h"

#include "Core/Logger.h"

#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>
#include <cstring>

namespace carrot::hot_reload {
    namespace fs = std::filesystem;

    void shader_watcher_t::init(const shader_reload_callback_t& callback) noexcept
    {
        _callback = callback;
        _initialized_ok = false;

        _inotify_fd = inotify_init1(IN_NONBLOCK);
        if (_inotify_fd == -1)
        {
            LOG_CORE_ERROR("inotify_init1 failed: {}", strerror(errno));
            return;
        }

        // Watch the SOURCE shaders directory (NOT bin)
#ifndef CARROT_SOURCE_ROOT
#error "CARROT_SOURCE_ROOT must be defined in CMake"
#endif

        const std::string shader_dir{ std::string(CARROT_SOURCE_ROOT) + "/shaders" };

        _watch_desc = inotify_add_watch(_inotify_fd, shader_dir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
        if (_watch_desc == -1)
        {
            LOG_CORE_ERROR("inotify_add_watch failed on {}: {}", shader_dir, strerror(errno));
            close(_inotify_fd);
            _inotify_fd = -1;
            return;
        }

        LOG_CORE_INFO("Shader hot-reload watching: {}", shader_dir);
        _initialized_ok = true;
    }

    void shader_watcher_t::shutdown() noexcept
    {
        if (_watch_desc != -1) inotify_rm_watch(_inotify_fd, _watch_desc);
        if (_inotify_fd != -1) close(_inotify_fd);
        _inotify_fd = -1;
        _watch_desc = -1;
        _callback = nullptr;
        _initialized_ok = false;
    }

    void shader_watcher_t::poll() noexcept
    {
        if (!_initialized_ok || _inotify_fd == -1) return;

        alignas(inotify_event) char buffer[8192];
        const ssize_t len{ read(_inotify_fd, buffer, sizeof(buffer)) };
        if (len <= 0) return;

        const inotify_event* event{ nullptr };
        for (char* ptr{ buffer }; ptr < buffer + len; /*ptr += sizeof(inotify_event) + event->len*/)
        {
            event = reinterpret_cast<inotify_event *>(ptr);

            if (event->len == 0) break; // malformed?

            std::string name(event->name);

            // Skip directories and irrelevant files early
            if (event->mask & IN_ISDIR) goto next;
            if (!name.ends_with(".vert.hlsl") && !name.ends_with(".frag.hlsl") &&
                !name.ends_with(".comp.hlsl") /* add others later */)
                goto next;

            if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE))
            {
                try_compile_and_notify(name);
            }

        next:
            ptr += sizeof(inotify_event) + event->len;
        }
    }

    void shader_watcher_t::recompile_all() noexcept
    {
        if (!_initialized_ok) return;

        const std::string dir = std::string(CARROT_SOURCE_ROOT) + "/shaders";

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
            CARROT_DXC_EXECUTABLE, profile, extra_flags, src_path, out_abs_path
        ) };

        LOG_GRAPHICS_INFO("[HotReload] Compiling: {} → {} ({})", filename, out_name, profile);

        const int result{ std::system(cmd.c_str()) };
        if (result == 0)
        {
            LOG_GRAPHICS_INFO("[HotReload] Success: {}", filename);
            usleep(50'000); // tiny debounce — helps when editor writes files in multiple steps
            if (_callback) _callback(out_rel_path);
        }
        else
        {
            LOG_GRAPHICS_ERROR("[HotReload] dxc failed (code {}): {}", WEXITSTATUS(result), filename);
        }
    }

    int shader_watcher_t::_inotify_fd = -1;
    int shader_watcher_t::_watch_desc = -1;
    shader_reload_callback_t shader_watcher_t::_callback;
    bool shader_watcher_t::_initialized_ok = false;
} // namespace carrot::hot_reload
