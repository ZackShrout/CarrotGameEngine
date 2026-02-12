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
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot {
    namespace {
        uint64_t                                    _last_tick_time{ 0 };
        uint32_t                                    _frame_counter{ 0 };
        float                                       _fps_timer{ 0.f };
        bool                                        _debug_overlay_initialized{ false };
        core::ce_application_t*                     _application{ nullptr };
    } // anonymous namespace

    // PUBLIC
    engine_t::engine_t() noexcept
    {
        constexpr uint32_t width{ 1280 };
        constexpr uint32_t height{ 720 };

        core::logger_t::init();
        window::create_primary_window(width, height, "Carrot Engine – Month 1");

        engine_config_t config{ load_engine_config() };

        _renderer = std::make_unique<renderer::renderer_t>(config.graphics);
        _audio_module = std::make_unique<audio::audio_module_t>(config.audio);
        _audio_module->init();

        audio::audio_command_t cmd{};
        cmd.type = audio::audio_command_type::play_sine;
        cmd.play_sine.frequency = 660.0f;
        cmd.play_sine.gain = 0.2f;

        _audio_module->engine().enqueue_command(cmd);

        cmd.type = audio::audio_command_type::play_sine;
        cmd.play_sine.frequency = 440.0f;
        cmd.play_sine.gain = 0.2f;

        _audio_module->engine().enqueue_command(cmd);

        cmd.type = audio::audio_command_type::play_sine;
        cmd.play_sine.frequency = 523.0f;
        // cmd.play_sine.frequency = 554.0f;
        cmd.play_sine.gain = 0.2f;

        _audio_module->engine().enqueue_command(cmd);

        cmd.type = audio::audio_command_type::set_bus_gain;
        cmd.set_bus_gain.bus = audio::audio_bus_id::sfx;
        cmd.set_bus_gain.gain = 1.f;

        _audio_module->engine().enqueue_command(cmd);

        cmd.type = audio::audio_command_type::set_voice_gain;
        cmd.set_voice_gain.voice_index = 0;
        cmd.set_voice_gain.gain = 0.2f;

        _audio_module->engine().enqueue_command(cmd);

        LOG_CORE_INFO("Carrot Engine Initialized (Pure RHI Mode)");
    }

    engine_t::~engine_t()
    {
        LOG_CORE_INFO("Shutting down...");

        hot_reload::shader_watcher_t::shutdown();
        _audio_module->shutdown();
        _audio_module.reset();
        _renderer.reset();
        window::destroy_primary_window();
        core::logger_t::shutdown();
    }

    void engine_t::run(core::ce_application_t* app)
    {
        auto& main_window = window::get_primary_window();
        _application = app;

        _last_tick_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

        // Bind the on_tick function in the engine's application class, to be inherited
        _on_tick += BIND_MEMBER(_application, on_tick);

        // Bind window events
        main_window._on_window_resized += BIND_LAMBDA([this](const events::window_resized_t& e) {
            _renderer->get_rhi()->resize(e._width, e._height);
        });

        // Bind input events
        main_window._on_key += BIND_MEMBER(_application, on_key);
        main_window._on_mouse_button += BIND_MEMBER(_application, on_mouse_button);
        main_window._on_mouse_moved += BIND_MEMBER(_application, on_mouse_moved);
        main_window._on_mouse_scrolled += BIND_MEMBER(_application, on_mouse_scrolled);

        while (!_should_quit && !main_window.should_close())
        {
            window::poll_events();
            hot_reload::shader_watcher_t::poll();
            tick();

            if (window::is_minimized()) continue;

            _renderer->begin_frame();
            _renderer->get_rhi()->record_frame();

            // Initialize debug overlay AFTER the first swapchain image exists
            if (!_debug_overlay_initialized)
            {
                // debug::init(_renderer);
                _debug_overlay_initialized = true;
            }

            _renderer->end_frame();
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

        _delta_time = static_cast<float>(static_cast<uint64_t>(now_ms) - _last_tick_time) / 1000.f;
        _last_tick_time = static_cast<uint64_t>(now_ms);

        _fps_timer += _delta_time;
        ++_frame_counter;

        if (_fps_timer >= 1.0f)
        {
            _current_fps = _frame_counter;
            _frame_counter = 0;
            _fps_timer -= 1.0f;
        }

        // if (_frame_counter % 10 == 0)
        // {
        //     for (uint32_t i = 0; i < 32; ++i)
        //     {
        //         audio::audio_command_t cmd{};
        //         cmd.type = audio::audio_command_type::play_sine;
        //         cmd.play_sine.frequency = 220.0f + (i * 10.0f);
        //         cmd.play_sine.gain = 0.1f;
        //
        //         _audio_module->engine().enqueue_command(cmd);
        //     }
        // }

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
        //     _renderer->get_rhi()->resize(800, 600);
        //     resized = true;
        // }
    }
} // namespace carrot
