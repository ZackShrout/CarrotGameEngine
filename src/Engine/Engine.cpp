//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Engine.h"

#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "Utils/MulticastDelegate.h"
#include "Window/Window.h"
#include "Core/Application.h"
#include "RHI/RHI.h"

namespace carrot {
    namespace {
        uint64_t                                    _last_tick_time{ 0 };
        uint32_t                                    _frame_counter{ 0 };
        float                                       _fps_timer{ 0.f };
        bool                                        _debug_overlay_initialized{ false };
        core::ce_application_t*                     _application{ nullptr };
        utils::multicast_delegate_t<void(float dt)> _on_tick;
    } // anonymous namespace

    // PUBLIC
    engine_t::engine_t() noexcept
    {
        core::logger_t::init();
        window::create_primary_window(1280, 720, "Carrot Engine – Month 1");

        // Create RHI context
        rhi::rhi_desc_t desc{};
        desc.api = rhi::graphics_api::vulkan;
        desc.window_handle = window::get_primary_window().get_wl_surface(); // TODO: Wayland is now hardcoded in...
        desc.width = 1280;
        desc.height = 720;
        desc.enable_debug_layers = true;
        _rhi_context = rhi::create_rhi_context(desc);

        if (!_rhi_context)
        {
            LOG_CORE_FATAL("Failed to create RHI context!");
            std::abort();
        }

        // hot_reload::shader_watcher_t::init([this]([[maybe_unused]] const std::string& spv_path) {
        //     _renderer->reload_pipeline();
        // });

        LOG_CORE_INFO("Carrot Engine Initialized (Pure RHI Mode)");
    }

    engine_t::~engine_t()
    {
        LOG_CORE_INFO("Shutting down...");

        hot_reload::shader_watcher_t::shutdown();
        _rhi_context.reset();
        window::destroy_primary_window();
        core::logger_t::shutdown();
    }

    void engine_t::run(core::ce_application_t* app)
    {
        const auto& main_window = window::get_primary_window();
        _application = app;

        _last_tick_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        // Bind the on_tick function in the engine's application class, to be inherited
        _on_tick.add(utils::single_delegate_t<void(float)>::bind<&core::ce_application_t::on_tick>(_application));

        while (!_should_quit && !main_window.should_close())
        {
            window::poll_events();
            hot_reload::shader_watcher_t::poll();
            tick();

            // _renderer->begin_frame();
            // _renderer->render_frame(); // temporary — just our spinning triangle for now

            _rhi_context->begin_frame();
            _rhi_context->record_frame();

            // Initialize debug overlay AFTER the first swapchain image exists
            // if (!_debug_overlay_initialized)
            // {
            //     debug::init(_renderer);
            //     _debug_overlay_initialized = true;
            // }

            // _renderer->end_frame();
            _rhi_context->end_frame();
        }
    }

    engine_t& engine_t::get() noexcept
    {
        static engine_t instance;
        return instance;
    }

    // PRIVATE
    void engine_t::tick()
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

        // debug::text(20.f, 30.f, "FPS: %u", _current_fps);
        // debug::text(20.f, 65.f, "Frame: %.3f ms", _delta_time * 1000.f);

        _on_tick.broadcast(_delta_time);

        // Resize test
        // static float seconds_counter{ 0.0f };
        // static int seconds{ 0 };
        // static bool resized{ false };
        // seconds_counter += _delta_time;
        //
        // if (seconds_counter >= 6.0f && !resized)
        // {
        //     _rhi_context->resize(800, 600);
        //     resized = true;
        // }
    }
} // namespace carrot
