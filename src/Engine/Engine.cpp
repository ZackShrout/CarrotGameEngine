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

        const window::window_id_t main_window_id{ window::get_main_window_id() };
        if (!window::has_window(main_window_id))
        {
            LOG_CORE_FATAL("Main window was not available");
            return static_cast<int>(exit_code::error);
        }
        if (_gameplay_window_id == window::invalid_window_id || !window::has_window(_gameplay_window_id))
            _gameplay_window_id = main_window_id;

        _running = true;
        _application = app;

        _last_tick_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

        // Bind the on_tick function in the engine's application class, to be inherited
        _on_tick += BIND_MEMBER(_application, on_tick);

        for (const window::window_id_t window_id: window::get_window_ids())
            bind_window_events(window_id, main_window_id);

        LOG_CORE_INFO("Starting application...");
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

            if (window::is_minimized(main_window_id)) continue;

            _renderer->begin_frame();
            render_world();
            render_ui();
            render_debug();
            render_log_console();
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

    void engine_t::render_debug()
    {
        // Initialize debug overlay AFTER the first swapchain image exists
        if (!_debug_overlay_initialized)
        {
            debug::init(_renderer.get(), _vfs);
            _debug_overlay_initialized = debug::is_initialized();
        }

        const renderer::renderer_stats_t& stats{ _renderer->get_last_completed_stats() };
        const renderer::camera_2d_t& active_camera{ _renderer->get_camera_2d() };
        const renderer::resolved_camera_2d_t resolved_camera{ _renderer->resolve_camera_2d() };
        const input::controller_debug_snapshot_t controller_snapshot{ _controller_manager.debug_snapshot() };

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
        debug::text(16.f,
                    268.f,
                    "Controllers: %u connected | Active Slot: %s",
                    controller_snapshot.connected_gamepad_count,
                    controller_snapshot.active_gamepad_index.has_value()
                        ? std::to_string(*controller_snapshot.active_gamepad_index).c_str()
                        : "None");
        debug::text(16.f,
                    296.f,
                    "Stable Left Stick: %.2f, %.2f | South: %s",
                    static_cast<double>(controller_snapshot.active_gamepad.left_stick().x),
                    static_cast<double>(controller_snapshot.active_gamepad.left_stick().y),
                    controller_snapshot.active_gamepad.is_pressed(input::gamepad_button_t::south) ? "Down" : "Up");
        debug::text(16.f,
                    324.f,
                    "Raw Left Stick: %.2f, %.2f | South: %s | Release Pending: %.3f s",
                    static_cast<double>(controller_snapshot.raw_active_gamepad.left_stick().x),
                    static_cast<double>(controller_snapshot.raw_active_gamepad.left_stick().y),
                    controller_snapshot.raw_active_gamepad.is_pressed(input::gamepad_button_t::south) ? "Down" : "Up",
                    static_cast<double>(controller_snapshot.south_release_pending_seconds));

        const world::world_presentation_t& presentation{ _world.presentation() };
        const world::collision_debug_view_t& debug_view{ _world.collision_debug_view() };
        const float viewport_height{ static_cast<float>(resolved_camera.viewport_rect_px.size.y) };
        const float legend_y_start{ std::max(352.f, viewport_height - 72.f) };
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

        const ui::ui_widget_t* focused_widget{ _ui_module->get_focused_widget() };
        struct ui_stack_entry_t
        {
            const ui::ui_widget_t* widget{ nullptr };
            uint32_t depth{ 0u };
        };

        std::vector<ui_stack_entry_t> stack;
        stack.push_back({ .widget = root, .depth = 0u });
        uint32_t tree_widget_count{ 0u };
        uint32_t tree_focusable_count{ 0u };
        uint32_t tree_max_depth{ 0u };

        while (!stack.empty())
        {
            const ui_stack_entry_t entry{ stack.back() };
            stack.pop_back();
            const ui::ui_widget_t* widget{ entry.widget };

            if (!widget)
                continue;

            ++tree_widget_count;
            tree_max_depth = std::max(tree_max_depth, entry.depth);
            if (widget->can_receive_focus() && !widget->is_collapsed())
                ++tree_focusable_count;

            for (const auto& child : widget->get_children())
            {
                stack.push_back({
                    .widget = child.get(),
                    .depth = entry.depth + 1u
                });
            }

            if (widget == root || widget->is_collapsed())
                continue;

            const bool is_focused{ widget == focused_widget };
            if (!widget->can_receive_focus() && !is_focused)
                continue;

            const ui::ui_rect_t& bounds{ widget->get_layout_bounds() };
            if (bounds.width <= 0.f || bounds.height <= 0.f)
                continue;

            ui::ui_debug_visual_style_t visual_style;
            if (!widget->get_debug_visual_style(visual_style))
                continue;

            const float border_thickness{ std::max(0.f, visual_style.border_thickness) };
            const uint32_t fill_color{ is_focused ? visual_style.focused_fill_color : visual_style.fill_color };
            const uint32_t border_color{ is_focused ? visual_style.focused_border_color : visual_style.border_color };

            // Border
            _renderer->draw_solid_quad({
                .x = bounds.x - border_thickness,
                .y = bounds.y - border_thickness,
                .width = bounds.width + (2.f * border_thickness),
                .height = bounds.height + (2.f * border_thickness),
                .layer = renderer::render_layer_t::ui,
                .color = border_color,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });

            // Fill
            _renderer->draw_solid_quad({
                .x = bounds.x,
                .y = bounds.y,
                .width = bounds.width,
                .height = bounds.height,
                .layer = renderer::render_layer_t::ui,
                .color = fill_color,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });
        }

        const std::string focused_name{ focused_widget ? std::string{ focused_widget->get_debug_name() } : std::string{ "<none>" } };
        const uint64_t focused_id{ focused_widget ? focused_widget->get_id() : 0ull };
        const ui::ui_input_ownership_mode_t ownership_mode{ _ui_module->get_effective_input_ownership_mode() };
        const char* ownership_mode_text{ "unknown" };
        switch (ownership_mode)
        {
            case ui::ui_input_ownership_mode_t::passthrough: ownership_mode_text = "passthrough"; break;
            case ui::ui_input_ownership_mode_t::ui_priority: ownership_mode_text = "ui_priority"; break;
            case ui::ui_input_ownership_mode_t::ui_exclusive: ownership_mode_text = "ui_exclusive"; break;
            default: break;
        }

        constexpr float panel_x{ 16.f };
        constexpr float panel_y{ 352.f };
        constexpr float panel_width{ 420.f };
        constexpr float line_height{ 18.f };

        const std::vector<std::string>& nav_stream{ _ui_module->get_debug_navigation_events() };
        const float panel_height{ 116.f + (line_height * static_cast<float>(nav_stream.size())) };

        _renderer->draw_solid_quad({
            .x = panel_x,
            .y = panel_y,
            .width = panel_width,
            .height = panel_height,
            .layer = renderer::render_layer_t::ui,
            .color = 0x00141414u,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        debug::text_colored(panel_x + 8.f, panel_y + 8.f, 0xFF00D9FFu, "UI Debug");
        debug::text_colored(panel_x + 8.f, panel_y + 28.f, 0xFFD8D8D8u, "Focus ID: %llu", focused_id);
        debug::text_colored(panel_x + 8.f, panel_y + 46.f, 0xFFD8D8D8u, "Focus Name: %s", focused_name.c_str());
        debug::text_colored(panel_x + 8.f,
                            panel_y + 64.f,
                            0xFFB6B6B6u,
                            "Tree: widgets=%u focusable=%u max_depth=%u",
                            tree_widget_count,
                            tree_focusable_count,
                            tree_max_depth);
        debug::text_colored(panel_x + 8.f, panel_y + 82.f, 0xFFB6B6B6u, "Ownership: %s", ownership_mode_text);
        debug::text_colored(panel_x + 8.f, panel_y + 100.f, 0xFF44D8FFu, "Nav Stream:");

        for (size_t index{ 0u }; index < nav_stream.size(); ++index)
        {
            debug::text_colored(panel_x + 24.f,
                                panel_y + 118.f + (line_height * static_cast<float>(index)),
                                0xFF9AC0FFu,
                                "%s",
                                nav_stream[index].c_str());
        }
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

        return assets::audio_asset_manifest_importer_t::import(doc, _asset_manager->audio().registry(), _vfs);
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
