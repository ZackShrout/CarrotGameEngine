//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Engine.h"

#include "Assets/AssetService.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Sprite/SpriteAnimator.h"
#include "Assets/Sprite/SpriteAsset.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Audio/Audio.h"
#include "Core/Application.h"
#include "Core/EnginePaths.h"
#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "Renderer/RendererService.h"
#include "Renderer/Draw/SpriteDrawInfo.h"
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

        // Load built-in assets
        register_builtin_audio_assets();
        register_builtin_texture_assets();
        register_builtin_sprite_assets();
        register_builtin_tilemap_assets();
        build_test_sprite();

        if (const assets::loaded_tilemap_asset_t* tilemap{ _asset_manager->tilemaps().get("tilemap.test.overworld") })
        {
            _test_tilemap_overworld = tilemap;
            const assets::tilemap_asset_t& map{ tilemap->tilemap() };

            uint32_t object_count{ 0 };
            for (const assets::tilemap_layer_t& layer : map.layers())
            {
                if (layer.kind == assets::tilemap_layer_kind_t::object)
                    object_count += static_cast<uint32_t>(layer.objects.size());
            }

            LOG_ASSET_INFO(
                "Loaded tilemap '{}': {}x{} tiles, tile size {}x{}, layers={}, tilesets={}, objects={}",
                tilemap->record()->logical_id,
                map.width(),
                map.height(),
                map.tile_width(),
                map.tile_height(),
                map.layers().size(),
                map.tilesets().size(),
                object_count
            );
        }
        else
        {
            LOG_ASSET_WARN("Test tilemap lookup failed for 'tilemap.test.overworld'");
        }

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
        _vraden_animator.update(_delta_time);
        _kelvara_animator.update(_delta_time);

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

        if (!botan || !vraden || !orange || !logo)
            return;

        if (_test_tilemap_overworld && _test_tilemap_overworld->valid())
        {
            _renderer->draw_tilemap({
                .tilemap = _test_tilemap_overworld,
                .origin = { 320.f, 96.f },
                .layer = renderer::render_layer_t::world_back,
                .order_in_layer = -100,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
                .color = 0xFFFFFFFFu
            });
        }

        static float x_offset{ 0.0f };
        static bool moving_right{ true };

        float next_offset{ moving_right ? x_offset + 0.01f : x_offset - 0.01f };

        if (next_offset > 1.0f)
        {
            moving_right = false;
            next_offset = 1.0f;
        }
        else if (next_offset < -1.0f)
        {
            moving_right = true;
            next_offset = -1.0f;
        }

        x_offset = next_offset;

        // ─────────────────────────────────────────────────────────────────────────────
        // Corner parity checks
        // ─────────────────────────────────────────────────────────────────────────────

        renderer::textured_quad_draw_info_t top_left_botan{ };
        top_left_botan.texture = botan->texture.get();
        top_left_botan.x = 64.f;
        top_left_botan.y = 36.f;
        top_left_botan.width = 180.f;
        top_left_botan.height = 140.f;
        top_left_botan.layer = renderer::render_layer_t::background;
        top_left_botan.color = 0xFFFF0000u;
        top_left_botan.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(top_left_botan);

        renderer::textured_quad_draw_info_t top_right_vraden{ };
        top_right_vraden.texture = vraden->texture.get();
        top_right_vraden.x = 1024.f;
        top_right_vraden.y = 36.f;
        top_right_vraden.width = 180.f;
        top_right_vraden.height = 140.f;
        top_right_vraden.layer = renderer::render_layer_t::background;
        top_right_vraden.color = 0xFFFF00FFu;
        top_right_vraden.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(top_right_vraden);

        renderer::textured_quad_draw_info_t bottom_left_vraden{ };
        bottom_left_vraden.texture = vraden->texture.get();
        bottom_left_vraden.x = 64.f;
        bottom_left_vraden.y = 576.f;
        bottom_left_vraden.width = 180.f;
        bottom_left_vraden.height = 140.f;
        bottom_left_vraden.layer = renderer::render_layer_t::background;
        bottom_left_vraden.color = 0xFF00FF00u;
        bottom_left_vraden.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(bottom_left_vraden);

        renderer::textured_quad_draw_info_t bottom_right_botan{ };
        bottom_right_botan.texture = botan->texture.get();
        bottom_right_botan.x = 1024.f;
        bottom_right_botan.y = 576.f;
        bottom_right_botan.width = 180.f;
        bottom_right_botan.height = 140.f;
        bottom_right_botan.layer = renderer::render_layer_t::background;
        bottom_right_botan.color = 0xFF0000FFu;
        bottom_right_botan.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(bottom_right_botan);

        // ─────────────────────────────────────────────────────────────────────────────
        // Center motion + smooth sampling
        // ─────────────────────────────────────────────────────────────────────────────

        renderer::textured_quad_draw_info_t center_logo{ };
        center_logo.texture = logo->texture.get();
        center_logo.x = 512.f + (x_offset * 256.f);
        center_logo.y = 288.f;
        center_logo.width = 256.f;
        center_logo.height = 256;
        center_logo.layer = renderer::render_layer_t::actors;
        center_logo.order_in_layer = -20;
        center_logo.color = 0xFFFFFFFFu;
        center_logo.sampler_preset = renderer::quad_sampler_preset_t::smooth_clamp;
        _renderer->draw_textured_quad(center_logo);

        // ─────────────────────────────────────────────────────────────────────────────
        // UV test: use only the upper-left quarter of the logo texture
        // ─────────────────────────────────────────────────────────────────────────────

        renderer::textured_quad_draw_info_t uv_test_logo{ };
        uv_test_logo.texture = logo->texture.get();
        uv_test_logo.x = 288.f;
        uv_test_logo.y = 324.f;
        uv_test_logo.width = 128.f;
        uv_test_logo.height = 128.f;
        uv_test_logo.layer = renderer::render_layer_t::world_back;
        uv_test_logo.order_in_layer = -10;
        uv_test_logo.color = 0xFFFFFFFFu;
        uv_test_logo.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        uv_test_logo.u0 = 0.f;
        uv_test_logo.v0 = 0.f;
        uv_test_logo.u1 = 0.5f;
        uv_test_logo.v1 = 0.5f;
        _renderer->draw_textured_quad(uv_test_logo);

        // ─────────────────────────────────────────────────────────────────────────────
        // Alpha overlap test
        // Back quad first, then front quad with partial alpha
        // ─────────────────────────────────────────────────────────────────────────────

        renderer::textured_quad_draw_info_t alpha_back{ };
        alpha_back.texture = orange->texture.get();
        alpha_back.x = 864.f;
        alpha_back.y = 342.f;
        alpha_back.width = 128.f;
        alpha_back.height = 128.f;
        alpha_back.layer = renderer::render_layer_t::world_back;
        alpha_back.order_in_layer = 10;
        alpha_back.color = 0xFF00FFFFu;
        alpha_back.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(alpha_back);

        renderer::textured_quad_draw_info_t alpha_front{ };
        alpha_front.texture = orange->texture.get();
        alpha_front.x = 915.2f;
        alpha_front.y = 370.8f;
        alpha_front.width = 128.f;
        alpha_front.height = 128.f;
        alpha_front.layer = renderer::render_layer_t::world_front;
        alpha_front.order_in_layer = 10;
        alpha_front.color = 0x88FF0000u;
        alpha_front.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(alpha_front);

        // ─────────────────────────────────────────────────────────────────────────────
        // Edge clipping test: partially off-screen at top edge
        // ─────────────────────────────────────────────────────────────────────────────

        renderer::textured_quad_draw_info_t top_edge_clip{ };
        top_edge_clip.texture = orange->texture.get();
        top_edge_clip.x = 576.f;
        top_edge_clip.y = -62.f;
        top_edge_clip.width = 128.f;
        top_edge_clip.height = 128.f;
        top_edge_clip.layer = renderer::render_layer_t::world_front;
        top_edge_clip.order_in_layer = 100;
        top_edge_clip.color = 0xFFFFFFFFu;
        top_edge_clip.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;
        _renderer->draw_textured_quad(top_edge_clip);

        // ─────────────────────────────────────────────────────────────────────────────
        // Sprite Test
        // ─────────────────────────────────────────────────────────────────────────────

        if (const assets::sprite_frame_t* current{ _vraden_animator.current_frame() })
        {
            renderer::sprite_draw_info_t sprite_draw{ };
            sprite_draw.sprite = _test_sprite_vraden;
            sprite_draw.frame = current;
            sprite_draw.x = 96.f;
            sprite_draw.y = 324.f;
            sprite_draw.width = 180;
            sprite_draw.height = 140.f;
            sprite_draw.layer = renderer::render_layer_t::actors;
            sprite_draw.order_in_layer = 0;
            sprite_draw.color = 0xFFFFFFFFu;
            sprite_draw.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;

            _renderer->draw_sprite(sprite_draw);
        }
        else
        {
            LOG_CORE_WARN("Vraden animator returned no current frame.");
        }

        if (const assets::sprite_frame_t* current{ _kelvara_animator.current_frame() })
        {
            renderer::sprite_draw_info_t sprite_draw{ };
            sprite_draw.sprite = _test_sprite_kelvara;
            sprite_draw.frame = current;
            sprite_draw.x = 1024.f;
            sprite_draw.y = 324.f;
            sprite_draw.width = 180.f;
            sprite_draw.height = 140.f;
            sprite_draw.use_custom_pivot = true;
            sprite_draw.pivot = { 0.5f, 1.f };
            sprite_draw.layer = renderer::render_layer_t::actors;
            sprite_draw.order_in_layer = 10;
            sprite_draw.color = 0xFFFFFFFFu;
            sprite_draw.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;

            _renderer->draw_sprite(sprite_draw);
        }
        else
        {
            LOG_CORE_WARN("Kelvara animator returned no current frame.");
        }
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
        register_texture_asset_manifest("game://textures/vraden.texture.json");
        register_texture_asset_manifest("game://textures/kelvara.texture.json");
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

    void engine_t::register_builtin_sprite_assets()
    {
        register_sprite_asset_manifest("game://sprites/vraden.sprite.json");
        register_sprite_asset_manifest("game://sprites/kelvara.sprite.json");
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

    void engine_t::register_builtin_tilemap_assets()
    {
        register_tilemap_asset_manifest("game://tilemaps/test_overworld.tilemap.json");
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

    void engine_t::build_test_sprite()
    {
        _test_sprite_vraden = _asset_manager->sprites().get("sprite.vraden");

        if (_test_sprite_vraden)
        {
            _vraden_animator.set_sprite(_test_sprite_vraden);
            _vraden_animator.play("idle_down");
        }

        _test_sprite_kelvara = _asset_manager->sprites().get("sprite.kelvara");

        if (_test_sprite_kelvara)
        {
            _kelvara_animator.set_sprite(_test_sprite_kelvara);
            _kelvara_animator.play("excited_left");
        }
    }
} // namespace carrot
