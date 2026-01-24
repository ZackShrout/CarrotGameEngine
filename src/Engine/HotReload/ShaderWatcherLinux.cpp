//
// Created by zshrout on 1/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "ShaderWatcher.h"

#include "Core/Logger.h"

#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>
#include <cstring>

namespace carrot::hot_reload {
    namespace {
        struct state_t
        {
            int inotify_fd = -1;
            int watch_desc = -1;
            bool initialized = false;
        } _state;

        constexpr size_t _buffer_size{ 8192 };
    } // anonymous namespace

    void shader_watcher_t::init(const shader_reload_callback_t& callback) noexcept
    {
        _callback = callback;

        _state.inotify_fd = inotify_init1(IN_NONBLOCK);
        if (_state.inotify_fd == -1)
        {
            LOG_CORE_ERROR("inotify_init1 failed: {}", strerror(errno));
            return;
        }

        // Watch the SOURCE shaders directory (NOT bin)
#ifndef CARROT_SOURCE_ROOT
#error "CARROT_SOURCE_ROOT must be defined in CMake"
#endif

        const std::string shader_dir{ std::string(CARROT_SOURCE_ROOT) + "/shaders" };

        _state.watch_desc = inotify_add_watch(_state.inotify_fd, shader_dir.c_str(),
                                              IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
        if (_state.watch_desc == -1)
        {
            LOG_CORE_ERROR("inotify_add_watch failed on {}: {}", shader_dir, strerror(errno));
            close(_state.inotify_fd);
            _state.inotify_fd = -1;
            return;
        }

        LOG_CORE_INFO("Shader hot-reload watching: {}", shader_dir);
        _state.initialized = true;
    }

    void shader_watcher_t::shutdown() noexcept
    {
        if (_state.watch_desc != -1) inotify_rm_watch(_state.inotify_fd, _state.watch_desc);
        if (_state.inotify_fd != -1) close(_state.inotify_fd);
        _state.inotify_fd = -1;
        _state.watch_desc = -1;
        _callback = nullptr;
        _state.initialized = false;
    }

    void shader_watcher_t::poll() noexcept
    {
        if (!_state.initialized || _state.inotify_fd == -1) return;

        alignas(inotify_event) char buffer[_buffer_size];
        const ssize_t len{ read(_state.inotify_fd, buffer, sizeof(buffer)) };
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
} // namespace carrot::hot_reload
