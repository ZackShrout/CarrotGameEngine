//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Engine.h"

#include "Assets/AssetService.h"
#include "Audio/Audio.h"
#include "Audio/Sample/WavLoader.h"
#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "Utils/MulticastDelegate.h"
#include "Utils/File/FileUtils.h"
#include "Window/Window.h"
#include "Core/Application.h"
#include "RHI/RHI.h"
#include "Assets/Audio/AudioAssetImporter.h"
#include "Core/EnginePaths.h"

namespace carrot {
    namespace {
        uint64_t _last_tick_time{ 0 };
        uint32_t _frame_counter{ 0 };
        float _fps_timer{ 0.f };
        bool _debug_overlay_initialized{ false };
        core::ce_application_t* _application{ nullptr };
    } // anonymous namespace

    // PUBLIC
    engine_t::engine_t() noexcept
    {
        constexpr uint32_t width{ 1280 };
        constexpr uint32_t height{ 720 };

        core::logger_t::init();
        window::create_primary_window(width, height, "Carrot Engine – Month 1");

        engine_config_t config{ load_engine_config() };

        // RENDERER
        _renderer = std::make_unique<renderer::renderer_t>(config.graphics);

        // AUDIO
        _audio_module = std::make_unique<audio::audio_module_t>(config.audio);
        _audio_module->init();
        audio::audio_service_t::provide(_audio_module.get());

        // _audio_asset_registry = std::make_unique<assets::audio_asset_registry_t>();
        // assets::asset_service_t::provide(_audio_asset_registry.get());

        assets::asset_service_t::provide(&_asset_manager);
        assets::audio_asset_registry_t& audio_registry{ assets::asset_service_t::manager().audio() };

        // Start Audio Tests
        std::string_view path{ "assets/audio/victory.audio.json" };
        utils::json::json_document_t doc;

        if (!doc.parse_from_file(utils::file::resolve_asset_path(path).data()))
            LOG_ASSET_ERROR("Failed to parse audio asset file '{}'", path);

        assets::audio_asset_importer_t::import(doc, audio_registry);

        path = "assets/audio/jalen_theme.audio.json";

        if (!doc.parse_from_file(utils::file::resolve_asset_path(path).data()))
            LOG_ASSET_ERROR("Failed to parse audio asset file '{}'", path);

        assets::audio_asset_importer_t::import(doc, audio_registry);

        path = "assets/audio/hope_for_all_years.audio.json";

        if (!doc.parse_from_file(utils::file::resolve_asset_path(path).data()))
            LOG_ASSET_ERROR("Failed to parse audio asset file '{}'", path);

        assets::audio_asset_importer_t::import(doc, audio_registry);

        path = "assets/audio/oak_battle_theme.audio.json";

        if (!doc.parse_from_file(utils::file::resolve_asset_path(path).data()))
            LOG_ASSET_ERROR("Failed to parse audio asset file '{}'", path);

        assets::audio_asset_importer_t::import(doc, audio_registry);

        // End Audio Tests

        LOG_CORE_INFO("Carrot Engine Initialized (Pure RHI Mode)");
    }

    engine_t::~engine_t()
    {
        LOG_CORE_INFO("Shutting down...");

        hot_reload::shader_watcher_t::shutdown();
        // _audio_asset_registry.reset();
        assets::asset_service_t::reset();
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

        LOG_CORE_INFO("Starting application...");
        _application->start();

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

    void engine_t::configure_paths(const core::engine_paths_t& paths)
    {
        if (paths.engine_root)
            _vfs.mount("engine", *paths.engine_root, true);

        if (paths.game_root)
            _vfs.mount("game", *paths.game_root, true);

        if (paths.source_root)
            _vfs.mount("source", *paths.source_root, true);

        if (paths.save_root)
            _vfs.mount("save", *paths.save_root, false);
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

        _audio_module->update(_delta_time);


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
