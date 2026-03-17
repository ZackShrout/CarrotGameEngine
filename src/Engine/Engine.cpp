//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

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
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Core/EnginePaths.h"
#include "Utils/File/PlatformPaths.h"

namespace carrot {
    namespace {
        uint64_t _last_tick_time{ 0 };
        uint32_t _frame_counter{ 0 };
        float _fps_timer{ 0.f };
        bool _debug_overlay_initialized{ false };
        core::ce_application_t* _application{ nullptr };
    } // anonymous namespace

    // PUBLIC

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

    void engine_t::init()
    {
        return init(make_default_engine_paths());
    }

    void engine_t::init(const core::engine_paths_t& paths)
    {
        core::logger_t::init();

        configure_paths(paths);

        engine_config_t config{ load_engine_config(_vfs) };

        constexpr uint32_t width{ 1280 };
        constexpr uint32_t height{ 720 };

        window::create_primary_window(width, height, "Carrot Engine – Month 1");

        // RENDERER
        _renderer = std::make_unique<renderer::renderer_t>(_vfs, config.graphics);

        // AUDIO
        _audio_module = std::make_unique<audio::audio_module_t>(config.audio);
        _audio_module->init();
        audio::audio_service_t::provide(_audio_module.get());

        assets::asset_service_t::provide(&_asset_manager);

        // Start Audio Tests
        register_builtin_audio_assets();
        // End Audio Tests

        LOG_CORE_INFO("Carrot Engine Initialized (Pure RHI Mode)");
        _initialized = true;
    }

    int engine_t::run(core::ce_application_t* app)
    {
        if (!_initialized)
        {
            LOG_CORE_FATAL("engine_t::run() called before engine_t::init()");
            return static_cast<int>(exit_code::error);
        }

        if (!_renderer)
        {
            LOG_CORE_FATAL("Renderer not initialized");
            return static_cast<int>(exit_code::error);
        }

        if (!_renderer->get_rhi())
        {
            LOG_CORE_FATAL("RHI not initialized");
            return static_cast<int>(exit_code::error);
        }

        if (app == nullptr)
        {
            LOG_CORE_FATAL("Application not provided");
            return static_cast<int>(exit_code::error);
        }

        if (_running)
        {
            LOG_CORE_FATAL("engine_t::run() called while engine is already running");
            return static_cast<int>(exit_code::error);
        }

        core::platform::window_t& main_window{ window::get_primary_window() };

        _running = true;
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

        _running = false;

        return static_cast<int>(exit_code::success);
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

    core::engine_paths_t engine_t::make_default_engine_paths() noexcept
    {
        core::engine_paths_t paths{};

        const std::filesystem::path exe_dir{ utils::file::executable_directory() };
        const auto repo_root = find_repo_root(exe_dir);
        if (!repo_root)
            return paths;

        const std::filesystem::path engine_root = *repo_root / "assets";
        const std::filesystem::path game_root   = *repo_root / "Game" / "assets";
        const std::filesystem::path source_root = *repo_root / "Game" / "source";
        const std::filesystem::path save_root   = *repo_root / "Game" / "saved";

        if (std::filesystem::exists(engine_root))
            paths.engine_root = std::filesystem::weakly_canonical(engine_root);

        if (std::filesystem::exists(game_root))
            paths.game_root = std::filesystem::weakly_canonical(game_root);

        if (std::filesystem::exists(source_root))
            paths.source_root = std::filesystem::weakly_canonical(source_root);

        std::filesystem::create_directories(save_root);
        paths.save_root = std::filesystem::weakly_canonical(save_root);

        return paths;
    }

    std::optional<std::filesystem::path> engine_t::find_repo_root(std::filesystem::path start) noexcept
    {
        start = std::filesystem::weakly_canonical(start);

        while (!start.empty())
        {
            if (std::filesystem::exists(start / ".git") || std::filesystem::exists(start / "CMakeLists.txt"))
                return start;

            const std::filesystem::path parent = start.parent_path();
            if (parent == start)
                break;

            start = parent;
        }

        return std::nullopt;
    }

    void engine_t::configure_paths(const core::engine_paths_t& paths)
    {
        LOG_CORE_INFO("Configuring engine paths...");
        LOG_CORE_INFO("Engine Root: {}", paths.engine_root.value_or("Not set").c_str());
        LOG_CORE_INFO("Game Root: {}", paths.game_root.value_or("Not set").c_str());
        LOG_CORE_INFO("Source Root: {}", paths.source_root.value_or("Not set").c_str());
        LOG_CORE_INFO("Save Root: {}", paths.save_root.value_or("Not set").c_str());

        if (paths.engine_root)
            _vfs.mount("engine", *paths.engine_root, true);

        if (paths.game_root)
            _vfs.mount("game", *paths.game_root, true);

        if (paths.source_root)
            _vfs.mount("source", *paths.source_root, true);

        if (paths.save_root)
            _vfs.mount("save", *paths.save_root, false);
    }

    void engine_t::register_builtin_audio_assets()
    {
        register_audio_asset_manifest("engine://audio/victory.audio.json");
        register_audio_asset_manifest("engine://audio/jalen_theme.audio.json");
        register_audio_asset_manifest("engine://audio/hope_for_all_years.audio.json");
        register_audio_asset_manifest("engine://audio/oak_battle_theme.audio.json");
    }

    bool engine_t::register_audio_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{ _asset_manager.vfs().resolve_native_path(manifest_uri) };

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse audio asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::audio_asset_manifest_importer_t::import(doc, _asset_manager.audio().registry(), _vfs);
    }
} // namespace carrot
