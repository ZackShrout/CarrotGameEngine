//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "Application.h"

#include "Engine/Window/Window.h"
#include "../RHI/Backends/Vulkan/VulkanRenderer.h"
#include "Engine/HotReload/ShaderWatcher.h"
#include "Engine/Debug/DebugOverlay.h"
#include "Engine/Utils/MulticastDelegate.h"

#include <chrono>

namespace carrot::core {
    namespace {
        void init()
        {
            window::create_primary_window(1280, 720, "Carrot Engine – Month 1");
            vulkan_renderer_t::init();
            hot_reload::shader_watcher_t::init([]([[maybe_unused]] const std::string& spv_path) {
                vulkan_renderer_t::reload_pipeline();
            });
        }

        void shutdown()
        {
            debug::shutdown();
            hot_reload::shader_watcher_t::shutdown();
            vulkan_renderer_t::shutdown();
            window::destroy_primary_window();
        }

        uint64_t                                    _last_tick_time{ 0 };
        uint32_t                                    _frame_counter{ 0 };
        float                                       _fps_timer{ 0.f };
        bool                                        _debug_overlay_initialized{ false };
        utils::multicast_delegate_t<void(float dt)> _on_tick;

    } // anonymous namespace

    application_t::application_t() noexcept
    {
        MESSAGE("Starting up...");
        init();
    }

    application_t::~application_t()
    {
        MESSAGE("Shutting down...");
        shutdown();
    }

    void application_t::run()
    {
        const auto& main_window = window::get_primary_window();

        _last_tick_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        // _on_tick.add(utils::single_delegate_t<void(float)>::bind([](const float dt) {
        //     MESSAGE("Tick: %.3f ms", dt * 1000.f);
        // }));

        while (!_should_quit && !main_window.should_close())
        {
            window::poll_events();
            hot_reload::shader_watcher_t::poll();
            tick();

            vulkan_renderer_t::begin_frame();
            vulkan_renderer_t::render_frame(); // temporary — just our spinning triangle for now

            // Initialize debug overlay AFTER the first swapchain image exists
            if (!_debug_overlay_initialized)
            {
                debug::init();
                _debug_overlay_initialized = true;
            }

            vulkan_renderer_t::end_frame();
        }
    }

    application_t& application_t::get() noexcept
    {
        static application_t instance;
        return instance;
    }

    void application_t::tick()
    {
        const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        _delta_time = static_cast<float>(now_ms - _last_tick_time) / 1000.f;
        _last_tick_time = now_ms;

        _fps_timer += _delta_time;
        ++_frame_counter;

        if (_fps_timer >= 1.0f)
        {
            _current_fps = _frame_counter;
            _frame_counter = 0;
            _fps_timer -= 1.0f;
        }

        debug::text(20.f, 30.f, "FPS: %u", _current_fps);
        debug::text(20.f, 65.f, "Frame: %.3f ms", _delta_time * 1000.f);

        _on_tick.broadcast(_delta_time);
    }
} // namespace carrot
