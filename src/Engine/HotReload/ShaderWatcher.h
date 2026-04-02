//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include <functional>
#include <string>

namespace carrot::hot_reload {
    using shader_reload_callback_t = std::function<void(const std::string& spv_path)>;

    class shader_watcher_t
    {
    public:
        shader_watcher_t() = delete;
        ~shader_watcher_t() = delete;

        static void init(const shader_reload_callback_t& callback) noexcept;
        static void shutdown() noexcept;
        static void poll() noexcept; // call every frame from application_t::run()

        static void recompile_all() noexcept;
        static void try_compile_and_notify(const std::string& filename) noexcept;

    private:
        static shader_reload_callback_t _callback;
    };
} // namespace carrot::hot_reload
