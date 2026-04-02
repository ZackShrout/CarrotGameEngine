//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Engine.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetService.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Audio/Audio.h"
#include "Core/Application.h"
#include "Core/EnginePaths.h"
#include "Core/GameContext.h"
#include "Core/GameView.h"
#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "RHI/RHI.h"
#include "Renderer/RendererService.h"
#include "Utils/File/FileUtils.h"
#include "Utils/File/PlatformPaths.h"
#include "Utils/MulticastDelegate.h"
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
            _asset_manager->clear(); // optional but good
            _asset_manager.reset(); // destroys loaded textures while RHI/device still alive
        }

        debug::shutdown();

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

        // CAMERA (test code)
        renderer::camera_2d_t camera{ };
        camera.position = { 0.f, 0.f };
        camera.design_view_size = { static_cast<float>(width), static_cast<float>(height) };
        camera.sizing_mode = renderer::camera_2d_sizing_mode_t::fixed_aspect_letterbox;
        camera.zoom = 1.f;

        _renderer->set_camera_2d(camera);

        // AUDIO
        _audio_module = std::make_unique<audio::audio_module_t>(config.audio);
        _audio_module->init();
        audio::audio_service_t::provide(_audio_module.get());

        _asset_manager = std::make_unique<assets::asset_manager_t>(_vfs, *_renderer->get_rhi());
        assets::asset_service_t::provide(_asset_manager.get());

        // Discover and register supported asset manifests under engine:// and game://
        discover_and_register_assets();

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
        core::game_view_t game_view{ *_renderer };
        core::game_context_t game{
            .world = _world,
            .assets = *_asset_manager,
            .view = game_view
        };
        _application->start(game);

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
                debug::init(_renderer.get(), _vfs);
                _debug_overlay_initialized = debug::is_initialized();
            }

            const renderer::renderer_stats_t& stats{ _renderer->get_last_completed_stats() };
            const renderer::camera_2d_t& active_camera{ _renderer->get_camera_2d() };
            const renderer::resolved_camera_2d_t resolved_camera{ _renderer->resolve_camera_2d() };

            debug::text(16.f, 16.f, "Carrot Debug Text V1");
            debug::text(16.f, 44.f, "Backend: %s", rhi::graphics_api_to_string(_renderer->get_graphics_api()).data());
            debug::text(16.f, 72.f, "Camera: %s", renderer::camera_2d_sizing_mode_to_string(active_camera.sizing_mode));
            debug::text(16.f, 100.f, "Viewport: %u,%u %ux%u",
                        resolved_camera.viewport_rect_px.position.x,
                        resolved_camera.viewport_rect_px.position.y,
                        resolved_camera.viewport_rect_px.size.x,
                        resolved_camera.viewport_rect_px.size.y);
            debug::text(16.f, 128.f, "FPS: %u", _current_fps);
            debug::text(16.f, 156.f, "Draw Calls: %u", stats.draw_calls);
            debug::text(16.f, 184.f, "Quads: %u", stats.textured_quad_count);
            debug::text(16.f, 212.f, "Batches: %u", stats.textured_quad_batch_count);
            debug::text(16.f, 240.f, "Frame: %llu", static_cast<unsigned long long>(_renderer->get_frame_index()));

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
        _world.update(_delta_time);

        _on_tick.broadcast(_delta_time);
    }

    void engine_t::render_world()
    {
        _renderer->draw_world(_world);
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

    bool engine_t::discover_and_register_assets()
    {
        const assets::discovered_asset_manifests_t manifests{
            assets::asset_discovery_t::discover_supported_manifests(_asset_manager->vfs())
        };

        LOG_ASSET_INFO(
            "Discovered {} asset manifest(s): audio={}, textures={}, sprites={}, tilemaps={}, scenes={}",
            manifests.total_count(),
            manifests.audio.size(),
            manifests.textures.size(),
            manifests.sprites.size(),
            manifests.tilemaps.size(),
            manifests.scenes.size()
        );

        bool all_registered{ true };

        for (const std::string& manifest : manifests.audio)
            all_registered = register_audio_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest : manifests.textures)
            all_registered = register_texture_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest : manifests.sprites)
            all_registered = register_sprite_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest : manifests.tilemaps)
            all_registered = register_tilemap_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest : manifests.scenes)
            all_registered = register_scene_asset_manifest(manifest) && all_registered;

        return all_registered;
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

    bool engine_t::register_sprite_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve sprite asset manifest '{}'", manifest_uri);
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse sprite asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::sprite_asset_manifest_importer_t::import(doc, _asset_manager->sprites().registry(), _vfs);
    }

    bool engine_t::register_tilemap_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve tilemap asset manifest '{}'", manifest_uri);
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse tilemap asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::tilemap_asset_manifest_importer_t::import(doc, _asset_manager->tilemaps().registry(), _vfs);
    }

    bool engine_t::register_scene_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve scene asset manifest '{}'", manifest_uri);
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse scene asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::scene_asset_manifest_importer_t::import(doc, _asset_manager->scenes().registry(), _vfs);
    }

} // namespace carrot
