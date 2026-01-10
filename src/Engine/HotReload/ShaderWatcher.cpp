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

        _watch_desc = inotify_add_watch(_inotify_fd, shader_dir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
        if (_watch_desc == -1)
        {
            LOG_CORE_ERROR("inotify_add_watch failed on {}: {}", shader_dir, strerror(errno));
            close(_inotify_fd);
            _inotify_fd = -1;
            return;
        }

        LOG_CORE_INFO("Shader hot-reload watching: {}", shader_dir);
    }

    void shader_watcher_t::shutdown() noexcept
    {
        if (_watch_desc != -1) inotify_rm_watch(_inotify_fd, _watch_desc);
        if (_inotify_fd != -1) close(_inotify_fd);
        _inotify_fd = -1;
        _watch_desc = -1;
        _callback = nullptr;
    }

    void shader_watcher_t::poll() noexcept
    {
        if (_inotify_fd == -1) return;

        char buffer[4096];
        const ssize_t len{ read(_inotify_fd, buffer, sizeof(buffer)) };
        if (len <= 0) return;

        const inotify_event* event{ nullptr };
        for (char* ptr{ buffer }; ptr < buffer + len; ptr += sizeof(inotify_event) + event->len)
        {
            event = reinterpret_cast<inotify_event *>(ptr);

            if (!(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO))) continue;

            std::string name(event->name);

            // Skip directories and non-shader files
            if (event->mask & IN_ISDIR) continue;
            if (!name.ends_with(".vert") && !name.ends_with(".frag") &&
                !name.ends_with(".tesc") && !name.ends_with(".tese") &&
                !name.ends_with(".geom") && !name.ends_with(".comp"))
            {
                continue;
            }

            // Build full source path
            const std::string source_path{ std::string(CARROT_SOURCE_ROOT) + "/shaders/" + name };

            // Build output SPIR-V path (relative to executable)
            const std::string spv_name = name + ".spv";
            const std::string spv_path = "shaders/" + spv_name;

            // Recompile
            std::string cmd = "glslangValidator -V \"" + source_path + "\" -o \"" + spv_path + "\"";

            LOG_GRAPHICS_INFO("[HotReload] Compiling: {}", name);
            int result{ system(cmd.c_str()) };

            if (result == 0)
            {
                LOG_GRAPHICS_INFO("[HotReload] Success: {}", name);
                // Small debounce
                usleep(50000);
                if (_callback) _callback(spv_path);
            }
            else
            {
                LOG_GRAPHICS_ERROR("[HotReload] Compile failed (exit {}): {}", WEXITSTATUS(result), name);
            }
        }
    }

    int shader_watcher_t::_inotify_fd = -1;
    int shader_watcher_t::_watch_desc = -1;
    shader_reload_callback_t shader_watcher_t::_callback;
} // namespace carrot::hot_reload
