//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Engine.h"

#include "Assets/AssetService.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Audio/Audio.h"
#include "Core/Application.h"
#include "Core/EnginePaths.h"
#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "Renderer/RendererService.h"
#include "RHI/RHI.h"
#include "Utils/MulticastDelegate.h"
#include "Utils/File/FileUtils.h"
#include "Utils/File/PlatformPaths.h"
#include "Window/Window.h"

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

        if (_renderer && _renderer->get_rhi())
            _renderer->get_rhi()->release_asset_references();

        assets::asset_service_t::reset();

        if (_asset_manager)
        {
            _asset_manager->clear();   // optional but good
            _asset_manager.reset();    // destroys loaded textures while RHI/device still alive
        }

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
        renderer::renderer_service_t::provide(_renderer.get());

        // AUDIO
        _audio_module = std::make_unique<audio::audio_module_t>(config.audio);
        _audio_module->init();
        audio::audio_service_t::provide(_audio_module.get());

        _asset_manager = std::make_unique<assets::asset_manager_t>(_vfs, *_renderer->get_rhi());
        assets::asset_service_t::provide(_asset_manager.get());

        // Load built-in assets
        register_builtin_audio_assets();
        register_builtin_texture_assets();

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
            render_world();

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

        if ((_renderer->get_frame_index() % 120) == 0)
        {
            const renderer::renderer_stats_t& stats{ _renderer->get_stats() };

            if (stats.draw_calls != _last_logged_renderer_stats.draw_calls ||
                stats.textured_quad_count != _last_logged_renderer_stats.textured_quad_count ||
                stats.textured_quad_batch_count != _last_logged_renderer_stats.textured_quad_batch_count ||
                stats.vertex_count != _last_logged_renderer_stats.vertex_count ||
                stats.index_count != _last_logged_renderer_stats.index_count)
            {
                LOG_GRAPHICS_INFO(
                    "Renderer stats: draws={}, quads={}, batches={}, verts={}, indices={}",
                    stats.draw_calls,
                    stats.textured_quad_count,
                    stats.textured_quad_batch_count,
                    stats.vertex_count,
                    stats.index_count
                );

                _last_logged_renderer_stats = stats;
            }
        }

        _on_tick.broadcast(_delta_time);
    }

    void engine_t::render_world()
    {
        const assets::loaded_texture_asset_t* botan{
            assets::asset_service_t::manager().textures().get("engine.botan_test")
        };
        const assets::loaded_texture_asset_t* vraden{
            assets::asset_service_t::manager().textures().get("engine.vraden_test")
        };
        const assets::loaded_texture_asset_t* orange{
            assets::asset_service_t::manager().textures().get("engine.16x16orange")
        };
        const assets::loaded_texture_asset_t* logo{
            assets::asset_service_t::manager().textures().get("engine.carrot_engine_logo_512")
        };

        renderer::textured_quad_draw_info_t quad1{ };
        quad1.texture = botan->texture.get();
        quad1.x = -0.9f;
        quad1.y = -0.9f;
        quad1.width = 0.3f;
        quad1.height = 0.3f;
        quad1.color = 0xFFFF0000u;
        quad1.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(quad1);

        renderer::textured_quad_draw_info_t quad2{ };
        quad2.texture = logo->texture.get();
        quad2.x = -0.2f;
        quad2.y = -0.2f;
        quad2.width = 0.4f;
        quad2.height = 0.4f;
        quad2.color = 0xFFFFFFFFu;
        quad2.sampler_preset = renderer::quad_sampler_preset_t::smooth_clamp;
        _renderer->draw_textured_quad(quad2);

        renderer::textured_quad_draw_info_t quad3{ };
        quad3.texture = vraden->texture.get();
        quad3.x = 0.5f;
        quad3.y = -0.9f;
        quad3.width = 0.3f;
        quad3.height = 0.3f;
        quad3.color = 0xFFFF00FFu;
        quad3.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(quad3);

        renderer::textured_quad_draw_info_t quad4{ };
        quad4.texture = vraden->texture.get();
        quad4.x = -0.9f;
        quad4.y = 0.5f;
        quad4.width = 0.3f;
        quad4.height = 0.3f;
        quad4.color = 0xFF00FF00u;
        quad4.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(quad4);

        renderer::textured_quad_draw_info_t quad5{ };
        quad5.texture = botan->texture.get();
        quad5.x = 0.5f;
        quad5.y = 0.5f;
        quad5.width = 0.3f;
        quad5.height = 0.3f;
        quad5.color = 0xFF0000FFu;
        quad5.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(quad5);
    }

    core::engine_paths_t engine_t::make_default_engine_paths() noexcept
    {
        core::engine_paths_t paths{ };

        const std::filesystem::path exe_dir{ utils::file::executable_directory() };
        const auto repo_root = find_repo_root(exe_dir);
        if (!repo_root)
            return paths;

        const std::filesystem::path engine_root{ *repo_root / "assets" };
        const std::filesystem::path& engine_src{ *repo_root };
        const std::filesystem::path game_root{ *repo_root / "src" / "Game" / "assets" };
        const std::filesystem::path source_root{ *repo_root / "src" / "Game" / "source" };
        const std::filesystem::path save_root{ *repo_root / "src" / "Game" / "saved" };

        if (std::filesystem::exists(engine_root))
            paths.engine_root = std::filesystem::weakly_canonical(engine_root);

        if (std::filesystem::exists(engine_src))
            paths.engine_src = std::filesystem::weakly_canonical(engine_src);

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
        LOG_CORE_INFO("Engine Root: {}", utils::file::to_log_string(paths.engine_root.value_or("Not set")));
        LOG_CORE_INFO("Engine Source: {}", utils::file::to_log_string(paths.engine_src.value_or("Not set")));
        LOG_CORE_INFO("Game Root: {}", utils::file::to_log_string(paths.game_root.value_or("Not set")));
        LOG_CORE_INFO("Source Root: {}", utils::file::to_log_string(paths.source_root.value_or("Not set")));
        LOG_CORE_INFO("Save Root: {}", utils::file::to_log_string(paths.save_root.value_or("Not set")));

        if (paths.engine_root)
            _vfs.mount("engine", *paths.engine_root, true);

        if (paths.engine_src)
            _vfs.mount("engine_src", *paths.engine_src, true);

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
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse audio asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::audio_asset_manifest_importer_t::import(doc, _asset_manager->audio().registry(), _vfs);
    }

    void engine_t::register_builtin_texture_assets()
    {
        register_texture_asset_manifest("engine://textures/botan_test.texture.json");
        register_texture_asset_manifest("engine://textures/vraden_test.texture.json");
        register_texture_asset_manifest("engine://textures/16x16orange.texture.json");
        register_texture_asset_manifest("engine://textures/carrot_engine_logo_512.texture.json");
    }

    bool engine_t::register_texture_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve texture asset manifest '{}'", manifest_uri);
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse texture asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::texture_asset_manifest_importer_t::import(doc, _asset_manager->textures().registry(), _vfs);
    }
} // namespace carrot
