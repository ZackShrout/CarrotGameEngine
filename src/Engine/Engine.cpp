//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Engine.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetService.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Font/FontAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Audio/Audio.h"
#include "Core/Application.h"
#include "Core/EnginePaths.h"
#include "Core/GameContext.h"
#include "Core/GameView.h"
#include "Core/Logger.h"
#include "Debug/DebugOverlay.h"
#include "HotReload/ShaderWatcher.h"
#include "RHI/RHI.h"
#include "Renderer/RendererService.h"
#include "RuntimeWindowSpecs.h"
#include "UI/UIService.h"
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

        void ensure_debug_overlay_initialized(renderer::renderer_t* renderer,
                                              const io::virtual_file_system_t& vfs) noexcept
        {
            if (_debug_overlay_initialized || renderer == nullptr)
                return;

            debug::init(renderer, vfs);
            _debug_overlay_initialized = debug::is_initialized();
        }

        [[nodiscard]] std::optional<collision::collision_aabb_t> object_collision_bounds(
            const world::world_object_t& object) noexcept
        {
            if (!object.transform || !object.collision)
                return std::nullopt;

            return collision::collision_aabb_t::from_center_extents(
                object.transform->position + object.collision->offset,
                object.collision->half_extents
            );
        }

        [[nodiscard]] collision::collision_aabb_t presentation_aabb(const collision::collision_aabb_t& bounds,
                                                                    const world::world_presentation_t& presentation)
            noexcept
        {
            return collision::collision_aabb_t::from_min_size(
                presentation.world_position_to_pixels(bounds.min),
                presentation.world_size_to_pixels(bounds.size())
            );
        }

        void append_unique(std::vector<std::string>& values, std::string_view value)
        {
            if (value.empty())
                return;

            if (std::ranges::find(values, value) != values.end())
                return;

            values.emplace_back(value);
        }

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

        if (_audio_module)
            _audio_module->shutdown();
        _audio_module.reset();
        audio::audio_service_t::reset();

        if (_ui_module)
            _ui_module->shutdown();
        _ui_module.reset();
        ui::ui_service_t::reset();

        _renderer.reset();
        renderer::renderer_service_t::reset();
        window::destroy_all_windows();
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

        _runtime_windows.clear();
        _gameplay_window_id = window::invalid_window_id;

        const std::vector<engine_runtime_window_spec_t> window_specs{ build_runtime_window_specs(width, height) };
        const auto main_spec_it{
            std::find_if(window_specs.begin(),
                         window_specs.end(),
                         [](const engine_runtime_window_spec_t& spec) { return spec.is_main_window; })
        };

        if (main_spec_it == window_specs.end() || !create_runtime_window(*main_spec_it))
        {
            LOG_CORE_FATAL("Failed to create main engine window");

            return;
        }

        const window::window_id_t main_window_id{ window::get_main_window_id() };
        if (!window::has_window(main_window_id))
        {
            LOG_CORE_FATAL("Failed to resolve main engine window id");
            return;
        }

        // RENDERER
        _renderer = std::make_unique<renderer::renderer_t>(_vfs, config.graphics, main_window_id);
        renderer::renderer_service_t::provide(_renderer.get());

        // UI
        _ui_module = std::make_unique<ui::ui_module_t>();
        _ui_module->init();
        ui::ui_service_t::provide(_ui_module.get());

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

        for (const engine_runtime_window_spec_t& spec: window_specs)
        {
            if (spec.is_main_window)
                continue;

            if (!create_runtime_window(spec))
            {
                LOG_CORE_WARN("Failed to create runtime window for role {}", static_cast<uint32_t>(spec.role));
            }
        }

        initialize_boot_pipeline();

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

        const window::window_id_t main_window_id{ window::get_main_window_id() };
        if (!window::has_window(main_window_id))
        {
            LOG_CORE_FATAL("Main window was not available");
            return static_cast<int>(exit_code::error);
        }
        if (_gameplay_window_id == window::invalid_window_id || !window::has_window(_gameplay_window_id))
            _gameplay_window_id = main_window_id;

        _running = true;
        _application_started = false;
        _application = app;
        _boot_pipeline.prewarm_plan = core::boot_prewarm_plan_t{};
        _boot_pipeline.prewarm_plan.font_ids.emplace_back("font.engine.roboto_regular");
        _application->describe_boot_prewarm(_boot_pipeline.prewarm_plan);

        _last_tick_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        core::game_view_t game_view{ *_renderer };
        core::game_context_t game{
            .world = _world,
            .assets = *_asset_manager,
            .view = game_view,
            .controllers = _controller_manager
        };
        _application->start(game);

        while (!_should_quit && !window::should_close(main_window_id))
        {
            window::poll_events();
            destroy_closed_auxiliary_windows();

            if (_should_quit || window::should_close(main_window_id))
                break;

            hot_reload::shader_watcher_t::poll();
            tick();

            if (!_application_started && advance_boot_pipeline())
                start_application(*_application, game, main_window_id);

            if (window::is_minimized(main_window_id)) continue;

            _renderer->begin_frame();
            if (_application_started)
            {
                render_world();
                render_ui();
                ensure_debug_overlay_initialized(_renderer.get(), _vfs);
                if (_application)
                    _application->on_render_overlay();
                if (_application && _application->show_debug_overlay())
                    render_debug();
            }
            else
            {
                render_boot_overlay();
            }
            render_log_console();
            _renderer->end_frame();
        }

        _running = false;

        return static_cast<int>(exit_code::success);
    }

    // PRIVATE
    void engine_t::initialize_boot_pipeline() noexcept
    {
        _boot_pipeline = boot_pipeline_t{};
        _boot_pipeline.stage = boot_stage_t::discovering_manifests;
        _boot_pipeline.completed_steps = 0u;
        _boot_pipeline.total_steps = 1u;
        _boot_pipeline.next_manifest_index = 0u;
        LOG_CORE_INFO("Initialized staged engine boot pipeline");
    }

    bool engine_t::advance_boot_pipeline()
    {
        if (_boot_pipeline.stage == boot_stage_t::complete)
            return true;

        if (!_asset_manager)
            return false;

        if (_boot_pipeline.stage == boot_stage_t::discovering_manifests)
        {
            const assets::discovered_asset_manifests_t manifests{
                assets::asset_discovery_t::discover_supported_manifests(_asset_manager->vfs())
            };

            _boot_pipeline.audio_manifests = manifests.audio;
            _boot_pipeline.font_manifests = manifests.fonts;
            _boot_pipeline.texture_manifests = manifests.textures;
            _boot_pipeline.sprite_manifests = manifests.sprites;
            _boot_pipeline.tilemap_manifests = manifests.tilemaps;
            _boot_pipeline.scene_manifests = manifests.scenes;
            _boot_pipeline.completed_steps = 1u;
            _boot_pipeline.total_steps = 1u + manifests.total_count() +
                                         _boot_pipeline.prewarm_plan.audio_ids.size() +
                                         _boot_pipeline.prewarm_plan.font_ids.size() +
                                         _boot_pipeline.prewarm_plan.texture_ids.size() +
                                         _boot_pipeline.prewarm_plan.sprite_ids.size() +
                                         _boot_pipeline.prewarm_plan.tilemap_ids.size();
            _boot_pipeline.next_manifest_index = 0u;
            _boot_pipeline.stage = boot_stage_t::registering_audio;

            LOG_ASSET_INFO(
                "Boot discovered {} asset manifest(s): audio={}, fonts={}, textures={}, sprites={}, tilemaps={}, scenes={}",
                manifests.total_count(),
                manifests.audio.size(),
                manifests.fonts.size(),
                manifests.textures.size(),
                manifests.sprites.size(),
                manifests.tilemaps.size(),
                manifests.scenes.size()
            );
        }

        constexpr size_t k_max_boot_steps_per_frame{ 8u };
        size_t steps_this_frame{ 0u };
        while (_boot_pipeline.stage != boot_stage_t::complete && steps_this_frame < k_max_boot_steps_per_frame)
        {
            std::vector<std::string>* manifest_group{ nullptr };
            bool (engine_t::*register_manifest)(std::string_view){ nullptr };
            boot_stage_t next_stage{ boot_stage_t::complete };

            switch (_boot_pipeline.stage)
            {
                case boot_stage_t::registering_audio:
                    manifest_group = &_boot_pipeline.audio_manifests;
                    register_manifest = &engine_t::register_audio_asset_manifest;
                    next_stage = boot_stage_t::registering_fonts;
                    break;
                case boot_stage_t::registering_fonts:
                    manifest_group = &_boot_pipeline.font_manifests;
                    register_manifest = &engine_t::register_font_asset_manifest;
                    next_stage = boot_stage_t::registering_textures;
                    break;
                case boot_stage_t::registering_textures:
                    manifest_group = &_boot_pipeline.texture_manifests;
                    register_manifest = &engine_t::register_texture_asset_manifest;
                    next_stage = boot_stage_t::registering_sprites;
                    break;
                case boot_stage_t::registering_sprites:
                    manifest_group = &_boot_pipeline.sprite_manifests;
                    register_manifest = &engine_t::register_sprite_asset_manifest;
                    next_stage = boot_stage_t::registering_tilemaps;
                    break;
                case boot_stage_t::registering_tilemaps:
                    manifest_group = &_boot_pipeline.tilemap_manifests;
                    register_manifest = &engine_t::register_tilemap_asset_manifest;
                    next_stage = boot_stage_t::registering_scenes;
                    break;
                case boot_stage_t::registering_scenes:
                    manifest_group = &_boot_pipeline.scene_manifests;
                    register_manifest = &engine_t::register_scene_asset_manifest;
                    next_stage = boot_stage_t::expanding_scene_prewarm;
                    break;
                case boot_stage_t::expanding_scene_prewarm:
                {
                    for (const std::string& scene_id : _boot_pipeline.prewarm_plan.scene_ids)
                    {
                        const assets::scene_asset_record_t* scene_record{ _asset_manager->scenes().registry().find(scene_id) };
                        if (!scene_record)
                        {
                            LOG_ASSET_WARN("Boot prewarm skipped unknown scene '{}'", scene_id);
                            continue;
                        }

                        append_unique(_boot_pipeline.prewarm_plan.tilemap_ids, scene_record->scene.tilemap_id);
                        append_unique(_boot_pipeline.prewarm_plan.sprite_ids, scene_record->scene.player_sprite_id);
                        append_unique(_boot_pipeline.prewarm_plan.audio_ids, scene_record->scene.initial_music_id);

                        if (const assets::sprite_asset_record_t* sprite_record{
                                _asset_manager->sprites().registry().find(scene_record->scene.player_sprite_id)
                            })
                        {
                            append_unique(_boot_pipeline.prewarm_plan.texture_ids, sprite_record->sprite.texture_id());
                        }
                    }

                    _boot_pipeline.stage = boot_stage_t::prewarming_audio;
                    _boot_pipeline.total_steps = 1u +
                                                 _boot_pipeline.audio_manifests.size() +
                                                 _boot_pipeline.font_manifests.size() +
                                                 _boot_pipeline.texture_manifests.size() +
                                                 _boot_pipeline.sprite_manifests.size() +
                                                 _boot_pipeline.tilemap_manifests.size() +
                                                 _boot_pipeline.scene_manifests.size() +
                                                 _boot_pipeline.prewarm_plan.audio_ids.size() +
                                                 _boot_pipeline.prewarm_plan.font_ids.size() +
                                                 _boot_pipeline.prewarm_plan.texture_ids.size() +
                                                 _boot_pipeline.prewarm_plan.sprite_ids.size() +
                                                 _boot_pipeline.prewarm_plan.tilemap_ids.size();
                    continue;
                }
                case boot_stage_t::prewarming_audio:
                    manifest_group = &_boot_pipeline.prewarm_plan.audio_ids;
                    register_manifest = nullptr;
                    next_stage = boot_stage_t::prewarming_fonts;
                    break;
                case boot_stage_t::prewarming_fonts:
                    manifest_group = &_boot_pipeline.prewarm_plan.font_ids;
                    register_manifest = nullptr;
                    next_stage = boot_stage_t::prewarming_textures;
                    break;
                case boot_stage_t::prewarming_textures:
                    manifest_group = &_boot_pipeline.prewarm_plan.texture_ids;
                    register_manifest = nullptr;
                    next_stage = boot_stage_t::prewarming_sprites;
                    break;
                case boot_stage_t::prewarming_sprites:
                    manifest_group = &_boot_pipeline.prewarm_plan.sprite_ids;
                    register_manifest = nullptr;
                    next_stage = boot_stage_t::prewarming_tilemaps;
                    break;
                case boot_stage_t::prewarming_tilemaps:
                    manifest_group = &_boot_pipeline.prewarm_plan.tilemap_ids;
                    register_manifest = nullptr;
                    next_stage = boot_stage_t::complete;
                    break;
                case boot_stage_t::complete:
                    return true;
                case boot_stage_t::pending:
                case boot_stage_t::discovering_manifests:
                    return false;
            }

            if (_boot_pipeline.next_manifest_index >= manifest_group->size())
            {
                _boot_pipeline.next_manifest_index = 0u;
                _boot_pipeline.stage = next_stage;
                continue;
            }

            const std::string_view manifest_uri{ (*manifest_group)[_boot_pipeline.next_manifest_index] };
            switch (_boot_pipeline.stage)
            {
                case boot_stage_t::registering_audio:
                case boot_stage_t::registering_fonts:
                case boot_stage_t::registering_textures:
                case boot_stage_t::registering_sprites:
                case boot_stage_t::registering_tilemaps:
                case boot_stage_t::registering_scenes:
                    (void)(this->*register_manifest)(manifest_uri);
                    break;
                case boot_stage_t::prewarming_audio:
                    (void)_asset_manager->audio().get(manifest_uri);
                    break;
                case boot_stage_t::prewarming_fonts:
                    (void)_asset_manager->fonts().get(manifest_uri);
                    break;
                case boot_stage_t::prewarming_textures:
                    (void)_asset_manager->textures().get(manifest_uri);
                    break;
                case boot_stage_t::prewarming_sprites:
                    (void)_asset_manager->sprites().get(manifest_uri);
                    break;
                case boot_stage_t::prewarming_tilemaps:
                    if (!_boot_pipeline.tilemap_prepare_future)
                    {
                        const assets::tilemap_asset_record_t* tilemap_record{
                            _asset_manager->tilemaps().registry().find(manifest_uri)
                        };
                        if (!tilemap_record)
                        {
                            LOG_ASSET_WARN("Boot prewarm skipped unknown tilemap '{}'", manifest_uri);
                            break;
                        }

                        _boot_pipeline.active_tilemap_prewarm_id = std::string{ manifest_uri };
                        _boot_pipeline.tilemap_prepare_future = std::make_unique<std::future<assets::tilemap_asset_prepare_result_t>>(
                            std::async(std::launch::async,
                                       [tilemap_record, vfs = &_asset_manager->vfs()]()
                                       {
                                           return assets::prepare_tilemap_asset(*tilemap_record, *vfs);
                                       })
                        );
                    }

                    if (_boot_pipeline.tilemap_prepare_future->wait_for(std::chrono::seconds{ 0 }) != std::future_status::ready)
                        return false;

                    {
                        assets::tilemap_asset_prepare_result_t prepared{ _boot_pipeline.tilemap_prepare_future->get() };
                        _boot_pipeline.tilemap_prepare_future.reset();

                        if (prepared.success())
                        {
                            auto realized{ assets::realize_prepared_tilemap_asset(std::move(prepared.asset), *_renderer->get_rhi()) };
                            if (realized.success())
                            {
                                if (const assets::tilemap_asset_record_t* tilemap_record{
                                        _asset_manager->tilemaps().registry().find(manifest_uri)
                                    })
                                {
                                    _asset_manager->tilemaps().cache_loaded(tilemap_record->id, std::move(realized.asset));
                                }
                            }
                        }
                    }

                    _boot_pipeline.active_tilemap_prewarm_id.clear();
                    break;
                case boot_stage_t::pending:
                case boot_stage_t::discovering_manifests:
                case boot_stage_t::expanding_scene_prewarm:
                case boot_stage_t::complete:
                    break;
            }
            ++_boot_pipeline.next_manifest_index;
            ++_boot_pipeline.completed_steps;
            ++steps_this_frame;
        }

        if (_boot_pipeline.stage == boot_stage_t::complete)
            LOG_CORE_INFO("Engine boot pipeline completed");

        return _boot_pipeline.stage == boot_stage_t::complete;
    }

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

        if (_audio_module)
            _audio_module->update(_delta_time);

        if (_ui_module)
            _ui_module->update(_delta_time);

        _controller_manager.update(_delta_time);
        _world.update(_delta_time);

        _on_tick.broadcast(_delta_time);
    }

    void engine_t::render_world()
    {
        _renderer->draw_world(_world);
    }

    void engine_t::render_boot_overlay() noexcept
    {
        if (!_renderer)
            return;

        const float viewport_width{ static_cast<float>(window::get_width()) };
        const float viewport_height{ static_cast<float>(window::get_height()) };
        if (viewport_width <= 0.f || viewport_height <= 0.f)
            return;

        _renderer->draw_overlay_solid_quad({
            .x = 0.f,
            .y = 0.f,
            .width = viewport_width,
            .height = viewport_height,
            .layer = renderer::render_layer_t::ui,
            .color = 0xFF000000u,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        constexpr float bar_width_fraction{ 0.34f };
        constexpr float bar_height{ 8.f };
        const float bar_width{ std::max(120.f, viewport_width * bar_width_fraction) };
        const float bar_x{ (viewport_width - bar_width) * 0.5f };
        const float bar_y{ viewport_height * 0.78f };
        const float fill_width{ std::clamp(boot_progress(), 0.f, 1.f) * bar_width };

        _renderer->draw_overlay_solid_quad({
            .x = bar_x,
            .y = bar_y,
            .width = bar_width,
            .height = bar_height,
            .layer = renderer::render_layer_t::ui,
            .color = 0xFF1E1E1Eu,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        if (fill_width > 0.001f)
        {
            _renderer->draw_overlay_solid_quad({
                .x = bar_x,
                .y = bar_y,
                .width = fill_width,
                .height = bar_height,
                .layer = renderer::render_layer_t::ui,
                .color = 0xFFFFFFFFu,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });
        }
    }

    std::string_view engine_t::boot_stage_label() const noexcept
    {
        switch (_boot_pipeline.stage)
        {
            case boot_stage_t::pending: return "pending";
            case boot_stage_t::discovering_manifests: return "discovering_manifests";
            case boot_stage_t::registering_audio: return "registering_audio";
            case boot_stage_t::registering_fonts: return "registering_fonts";
            case boot_stage_t::registering_textures: return "registering_textures";
            case boot_stage_t::registering_sprites: return "registering_sprites";
            case boot_stage_t::registering_tilemaps: return "registering_tilemaps";
            case boot_stage_t::registering_scenes: return "registering_scenes";
            case boot_stage_t::expanding_scene_prewarm: return "expanding_scene_prewarm";
            case boot_stage_t::prewarming_audio: return "prewarming_audio";
            case boot_stage_t::prewarming_fonts: return "prewarming_fonts";
            case boot_stage_t::prewarming_textures: return "prewarming_textures";
            case boot_stage_t::prewarming_sprites: return "prewarming_sprites";
            case boot_stage_t::prewarming_tilemaps: return "prewarming_tilemaps";
            case boot_stage_t::complete: return "complete";
        }

        return "unknown";
    }

    float engine_t::boot_progress() const noexcept
    {
        if (_boot_pipeline.stage == boot_stage_t::complete)
            return 1.f;

        if (_boot_pipeline.total_steps == 0u)
            return 0.f;

        return static_cast<float>(_boot_pipeline.completed_steps) /
               static_cast<float>(std::max<size_t>(_boot_pipeline.total_steps, 1u));
    }

    void engine_t::start_application(core::ce_application_t& app,
                                     core::game_context_t& game,
                                     const window::window_id_t main_window_id)
    {
        if (_application_started)
            return;

        // Bind the on_tick function in the engine's application class, to be inherited
        _on_tick += BIND_MEMBER(&app, on_tick);

        for (const window::window_id_t window_id: window::get_window_ids())
            bind_window_events(window_id, main_window_id);

        LOG_CORE_INFO("Starting application after staged boot pipeline (stage='{}', progress={:.0f}%)",
                      boot_stage_label(),
                      boot_progress() * 100.0f);
        app.start(game);
        _application_started = true;
    }

    void engine_t::render_debug()
    {
        // Initialize debug overlay AFTER the first swapchain image exists
        ensure_debug_overlay_initialized(_renderer.get(), _vfs);

        const renderer::renderer_stats_t& stats{ _renderer->get_last_completed_stats() };
        const renderer::resolved_camera_2d_t resolved_camera{ _renderer->resolve_camera_2d() };
        const input::controller_debug_snapshot_t controller_snapshot{ _controller_manager.debug_snapshot() };

        debug::text(16.f,
                    16.f,
                    "Backend: %s | FPS: %u | Frame: %llu",
                    rhi::graphics_api_to_string(_renderer->get_graphics_api()).data(),
                    _current_fps,
                    static_cast<unsigned long long>(_renderer->get_frame_index()));
        debug::text(16.f,
                    44.f,
                    "World Lights: %u | F+ Tiles: %u | Tile Light Refs: %u",
                    stats.world_point_light_count,
                    stats.forward_plus_tile_count,
                    stats.forward_plus_light_index_count);
        debug::text(16.f,
                    72.f,
                    "F+ Dropped Refs: %u | Tile Size: %u px",
                    stats.forward_plus_dropped_light_references,
                    static_cast<unsigned>(renderer::k_forward_plus_tile_size_px));
        debug::text(16.f,
                    100.f,
                    "Controllers: %u connected | Active Slot: %s | South: %s",
                    controller_snapshot.connected_gamepad_count,
                    controller_snapshot.active_gamepad_index.has_value()
                        ? std::to_string(*controller_snapshot.active_gamepad_index).c_str()
                        : "None",
                    controller_snapshot.active_gamepad.is_pressed(input::gamepad_button_t::south) ? "Down" : "Up");

        const world::world_presentation_t& presentation{ _world.presentation() };
        const world::collision_debug_view_t& debug_view{ _world.collision_debug_view() };
        const float viewport_height{ static_cast<float>(resolved_camera.viewport_rect_px.size.y) };
        const float legend_y_start{ std::max(128.f, viewport_height - 72.f) };
        const uint32_t object_legend_color{ 0xFF00FFFFu };

        debug::text_colored(16.f,
                            legend_y_start,
                            debug_view.map_collision_color,
                            "F2 Map Collision: %s",
                            debug_view.show_map_collision ? "ON" : "OFF");
        debug::text_colored(16.f,
                            legend_y_start + 28.f,
                            object_legend_color,
                            "F3 Object Colliders: %s",
                            debug_view.show_object_colliders ? "ON" : "OFF");

        if (debug_view.show_map_collision)
        {
            const debug::world_rect_style_t map_style{
                .color = debug_view.map_collision_color,
                .outline_thickness = debug_view.map_outline_thickness,
                .filled = false
            };

            for (const collision::static_collider_t& collider: _world.collision_world().static_colliders())
                debug::world_aabb(presentation_aabb(collider.bounds, presentation), map_style);
        }

        if (debug_view.show_object_colliders)
        {
            for (const world::world_object_t& object: _world.objects())
            {
                const std::optional<collision::collision_aabb_t> bounds{ object_collision_bounds(object) };
                if (!bounds || !object.collision || !object.collision->debug_display)
                    continue;

                const world::collision_debug_display_t& object_debug{ *object.collision->debug_display };
                debug::world_aabb(presentation_aabb(*bounds, presentation), debug::world_rect_style_t{
                                      .color = object_debug.color,
                                      .outline_thickness = object_debug.outline_thickness,
                                      .filled = object_debug.filled
                                  });
            }
        }

    }

    void engine_t::render_ui()
    {
        if (!_ui_module || !_renderer)
            return;

        ui::ui_root_widget_t* root{ _ui_module->get_root() };
        if (!root)
            return;

        window::window_id_t target_window_id{ _gameplay_window_id };
        if (target_window_id == window::invalid_window_id || !window::has_window(target_window_id))
            target_window_id = window::get_main_window_id();
        if (!window::has_window(target_window_id))
            return;

        const float viewport_width{ static_cast<float>(window::get_width(target_window_id)) };
        const float viewport_height{ static_cast<float>(window::get_height(target_window_id)) };
        if (viewport_width <= 0.f || viewport_height <= 0.f)
            return;

        root->layout_tree({
            .x = 0.f,
            .y = 0.f,
            .width = viewport_width,
            .height = viewport_height
        });
        root->render_tree(*_renderer);
    }

    void engine_t::render_log_console()
    {
        const auto log_window_it{
            std::find_if(_runtime_windows.begin(),
                         _runtime_windows.end(),
                         [](const runtime_window_instance_t& window) {
                             return window.role == engine_runtime_window_role_t::log_console;
                         })
        };

        if (log_window_it == _runtime_windows.end())
            return;

        const window::window_id_t log_window_id{ log_window_it->id };
        if (!window::has_window(log_window_id) || window::is_minimized(log_window_id))
            return;

        const float window_width{ static_cast<float>(window::get_width(log_window_id)) };
        const float window_height{ static_cast<float>(window::get_height(log_window_id)) };
        if (window_width <= 0.f || window_height <= 0.f)
            return;

        constexpr float outer_padding_x{ 12.f };
        constexpr float outer_padding_y{ 10.f };
        constexpr float line_height{ 18.f };
        constexpr float bottom_gutter{ 8.f };

        // Debug text is currently monospaced-ish enough that a simple width estimate is good
        // enough for console tuning. Better than a fixed hardcoded character count.
        constexpr float estimated_glyph_width{ 8.f };
        constexpr size_t min_visible_chars{ 12u };

        auto severity_color = [](const core::log_severity severity) noexcept -> uint32_t
        {
            switch (severity)
            {
                case core::log_severity::trace: return 0xFF7A7A7Au;
                case core::log_severity::debug: return 0xFFD8D8D8u;
                case core::log_severity::info:  return 0xFF4CE04Cu;
                case core::log_severity::warn:  return 0xFF44D8FFu;
                case core::log_severity::error: return 0xFF7070FFu;
                case core::log_severity::fatal: return 0xFFFFFFFFu;
                default:                        return 0xFFFFFFFFu;
            }
        };

        auto truncate_for_width = [](std::string text, const size_t max_chars) -> std::string {
            if (text.size() <= max_chars)
                return text;

            if (max_chars <= 3u)
                return text.substr(0, max_chars);

            text.resize(max_chars - 3u);
            text += "...";
            return text;
        };

        _renderer->draw_log_console_solid_quad({
            .x = 0.f,
            .y = 0.f,
            .width = window_width,
            .height = window_height,
            .layer = renderer::render_layer_t::ui,
            .color = 0x00121212u,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        const std::vector<core::log_message> messages{ core::logger_t::recent_messages() };

        const float content_width{ std::max(1.f, window_width - outer_padding_x * 2.f) };
        const float content_height{ std::max(1.f, window_height - outer_padding_y * 2.f - bottom_gutter) };

        const uint32_t max_visible_lines{
            static_cast<uint32_t>(std::max(1.f, std::floor(content_height / line_height)))
        };

        const size_t start_index{
            messages.size() > max_visible_lines
                ? messages.size() - static_cast<size_t>(max_visible_lines)
                : 0u
        };

        const size_t max_chars_per_line{
            std::max(min_visible_chars,
                     static_cast<size_t>(std::floor(content_width / estimated_glyph_width)))
        };

        // Bottom-anchor rows so resizing feels stable and the newest message lives at the bottom.
        const uint32_t visible_line_count{ static_cast<uint32_t>(messages.size() - start_index) };
        float y{
            window_height - outer_padding_y - bottom_gutter - (static_cast<float>(visible_line_count) * line_height)
        };

        for (size_t i = start_index; i < messages.size(); ++i)
        {
            const core::log_message& msg{ messages[i] };

            std::string rendered_message{
                std::format("[{} | {}] {}",
                            core::logger_t::category_to_string(msg.category),
                            core::logger_t::severity_to_string(msg.severity),
                            msg.message)
            };

            rendered_message = truncate_for_width(std::move(rendered_message), max_chars_per_line);

            debug::log_console_text_colored(
                outer_padding_x,
                y,
                severity_color(msg.severity),
                "%s",
                rendered_message.c_str()
            );

            y += line_height;
        }
    }

    std::vector<engine_runtime_window_spec_t> engine_t::build_runtime_window_specs(const uint32_t width,
                                                                                    const uint32_t height) const
    {
        return build_engine_runtime_window_specs(width, height);
    }

    bool engine_t::create_runtime_window(const engine_runtime_window_spec_t& spec)
    {
        const window::window_id_t window_id{ window::create_window(spec.create_desc) };
        if (window_id == window::invalid_window_id)
            return false;

        if (spec.is_main_window && !window::set_main_window(window_id))
        {
            (void)window::destroy_window(window_id);
            return false;
        }

        if (spec.receives_gameplay_input)
            _gameplay_window_id = window_id;

        runtime_window_instance_t instance{
            .role = spec.role,
            .id = window_id,
            .is_main_window = spec.is_main_window,
            .registered_for_presentation = false,
            .presentation_channel_mask = spec.presentation_channel_mask,
            .receives_gameplay_input = spec.receives_gameplay_input
        };

        if (_renderer && spec.register_for_presentation)
        {
            instance.registered_for_presentation =
                    _renderer->add_presentation_window(window_id, spec.presentation_channel_mask);
        }

        _runtime_windows.push_back(instance);

        if (!spec.is_main_window)
            LOG_CORE_INFO("Created auxiliary window with id {}", static_cast<unsigned long long>(window_id));

        return true;
    }

    void engine_t::bind_window_events(const window::window_id_t window_id, const window::window_id_t main_window_id)
    {
        core::platform::window_t* runtime_window{ window::get_window(window_id) };
        if (!runtime_window)
            return;

        if (window_id == main_window_id)
        {
            runtime_window->_on_window_resized += BIND_LAMBDA([this](const events::window_resized_t& e) {
                _renderer->get_rhi()->resize(e._width, e._height);
                });
        }

        runtime_window->_on_window_closed += BIND_MEMBER(_application, on_window_closed);
        runtime_window->_on_window_focus_changed += BIND_MEMBER(_application, on_window_focus_changed);
        runtime_window->_on_key += BIND_MEMBER(_application, on_key);

        const auto receives_gameplay_input{
            std::find_if(_runtime_windows.begin(),
                         _runtime_windows.end(),
                         [window_id](const runtime_window_instance_t& instance) {
                             return instance.id == window_id && instance.receives_gameplay_input;
                         }) != _runtime_windows.end()
        };
        if (receives_gameplay_input)
        {
            runtime_window->_on_mouse_button += BIND_MEMBER(_application, on_mouse_button);
            runtime_window->_on_mouse_moved += BIND_MEMBER(_application, on_mouse_moved);
            runtime_window->_on_mouse_scrolled += BIND_MEMBER(_application, on_mouse_scrolled);
        }
    }

    void engine_t::destroy_closed_auxiliary_windows()
    {
        bool refocus_main_window{ false };

        for (auto it = _runtime_windows.begin(); it != _runtime_windows.end();)
        {
            if (it->is_main_window)
            {
                ++it;
                continue;
            }

            const window::window_id_t id{ it->id };
            if (!window::has_window(id))
            {
                it = _runtime_windows.erase(it);
                continue;
            }

            if (window::should_close(id))
            {
                if (_renderer && it->registered_for_presentation)
                    (void)_renderer->remove_presentation_window(id);
                (void)window::destroy_window(id);
                it = _runtime_windows.erase(it);
                refocus_main_window = true;
                continue;
            }

            ++it;
        }

        if (refocus_main_window)
            window::request_focus(window::get_main_window_id());
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
        const std::filesystem::path game_root{ *repo_root / "src" / "Sandbox" / "assets" };
        const std::filesystem::path source_root{ *repo_root / "src" / "Sandbox" / "source" };
        const std::filesystem::path save_root{ *repo_root / "src" / "Sandbox" / "saved" };

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
            "Discovered {} asset manifest(s): audio={}, fonts={}, textures={}, sprites={}, tilemaps={}, scenes={}",
            manifests.total_count(),
            manifests.audio.size(),
            manifests.fonts.size(),
            manifests.textures.size(),
            manifests.sprites.size(),
            manifests.tilemaps.size(),
            manifests.scenes.size()
        );

        bool all_registered{ true };

        for (const std::string& manifest: manifests.audio)
            all_registered = register_audio_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest: manifests.fonts)
            all_registered = register_font_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest: manifests.textures)
            all_registered = register_texture_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest: manifests.sprites)
            all_registered = register_sprite_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest: manifests.tilemaps)
            all_registered = register_tilemap_asset_manifest(manifest) && all_registered;

        for (const std::string& manifest: manifests.scenes)
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

        return assets::audio_asset_manifest_importer_t::import(doc,
                                                               _asset_manager->audio().registry(),
                                                               _vfs,
                                                               manifest_uri);
    }

    bool engine_t::register_font_asset_manifest(std::string_view manifest_uri)
    {
        const std::optional<std::filesystem::path> native_path{
            _asset_manager->vfs().resolve_native_path(manifest_uri)
        };

        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve font asset manifest '{}'", manifest_uri);
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse font asset manifest '{}'", manifest_uri);
            return false;
        }

        return assets::font_asset_manifest_importer_t::import(doc, _asset_manager->fonts().registry(), _vfs, manifest_uri);
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

        return assets::texture_asset_manifest_importer_t::import(doc,
                                                                 _asset_manager->textures().registry(),
                                                                 _vfs,
                                                                 manifest_uri);
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

        return assets::sprite_asset_manifest_importer_t::import(doc,
                                                                _asset_manager->sprites().registry(),
                                                                _vfs,
                                                                manifest_uri);
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

        return assets::tilemap_asset_manifest_importer_t::import(doc,
                                                                 _asset_manager->tilemaps().registry(),
                                                                 _vfs,
                                                                 manifest_uri);
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

        if (!assets::scene_asset_manifest_importer_t::import(doc, _asset_manager->scenes().registry(), _vfs,
                                                             manifest_uri))
            return false;

        const utils::json::json_object_view_t root{ doc.root().as_object() };
        const std::string_view scene_id{ root.get_string("id") };
        const assets::scene_asset_record_t* record{ _asset_manager->scenes().registry().find(scene_id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Scene asset '{}' was imported but could not be found in the registry", scene_id);
            return false;
        }

        return _asset_manager->scenes().registry().validate_references(record[0],
                                                                       _asset_manager->tilemaps().registry(),
                                                                       _asset_manager->sprites().registry(),
                                                                       _asset_manager->audio().registry());
    }
} // namespace carrot
