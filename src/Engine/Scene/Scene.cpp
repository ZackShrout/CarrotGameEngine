//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Scene.h"

#include "Assets/AssetManager.h"
#include "Audio/Audio.h"
#include "Core/GameContext.h"
#include "Core/GameView.h"
#include "Debug/DebugOverlay.h"
#include "Window/Window.h"
#include "World/Controllers/InteractionController.h"
#include "World/Controllers/PlayerController.h"
#include "World/SceneLoader.h"
#include "World/World.h"

namespace carrot::scene {
    namespace {
        constexpr float k_transition_diagnostics_linger_seconds{ 1.25f };
        constexpr float k_transition_diagnostics_font_size{ 14.0f };

        [[nodiscard]] float apply_dead_zone_axis(const float current,
                                                 const float target,
                                                 const float half_dead_zone) noexcept
        {
            if (half_dead_zone <= 0.f)
                return target;

            const float delta{ target - current };
            if (std::fabs(delta) <= half_dead_zone)
                return current;

            return target - (std::signbit(delta) ? -half_dead_zone : half_dead_zone);
        }

        [[nodiscard]] world::world_object_t* find_scene_player(world::world_t& world,
                                                               const assets::scene_asset_t& scene) noexcept
        {
            return world.find_object_by_name(scene.player_name);
        }

        [[nodiscard]] const world::world_object_t* find_spawn_marker(const world::world_t& world,
                                                                     std::string_view marker_name) noexcept
        {
            return world.find_object_by_name(marker_name);
        }

        [[nodiscard]] scene_camera_override_t make_scene_camera_override(const assets::scene_camera_defaults_t& camera) noexcept
        {
            return scene_camera_override_t{
                .zoom = camera.zoom,
                .follow_mode = camera.follow_mode,
                .initial_target_policy = camera.initial_target_policy,
                .dead_zone_size_world = camera.dead_zone_size_world,
                .follow_smoothing = camera.follow_smoothing
            };
        }
    } // namespace

    std::string_view to_string(const scene_runtime_state_t state) noexcept
    {
        switch (state)
        {
            case scene_runtime_state_t::idle: return "idle";
            case scene_runtime_state_t::loading: return "loading";
            case scene_runtime_state_t::active: return "active";
            case scene_runtime_state_t::transitioning: return "transitioning";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_transition_phase_t phase) noexcept
    {
        switch (phase)
        {
            case scene_transition_phase_t::none: return "none";
            case scene_transition_phase_t::preparing: return "preparing";
            case scene_transition_phase_t::loading: return "loading";
            case scene_transition_phase_t::activating: return "activating";
            case scene_transition_phase_t::finalizing: return "finalizing";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_transition_overlay_style_t style) noexcept
    {
        switch (style)
        {
            case scene_transition_overlay_style_t::inherit: return "inherit";
            case scene_transition_overlay_style_t::none: return "none";
            case scene_transition_overlay_style_t::fade: return "fade";
            case scene_transition_overlay_style_t::loading_screen: return "loading_screen";
            case scene_transition_overlay_style_t::wipe: return "wipe";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_transition_wipe_direction_t direction) noexcept
    {
        switch (direction)
        {
            case scene_transition_wipe_direction_t::left_to_right: return "left_to_right";
            case scene_transition_wipe_direction_t::right_to_left: return "right_to_left";
            case scene_transition_wipe_direction_t::top_to_bottom: return "top_to_bottom";
            case scene_transition_wipe_direction_t::bottom_to_top: return "bottom_to_top";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_camera_projection_mode_t mode) noexcept
    {
        switch (mode)
        {
            case scene_camera_projection_mode_t::orthographic: return "orthographic";
            case scene_camera_projection_mode_t::perspective: return "perspective";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_camera_bounds_mode_t mode) noexcept
    {
        switch (mode)
        {
            case scene_camera_bounds_mode_t::none: return "none";
            case scene_camera_bounds_mode_t::scene_extents: return "scene_extents";
        }

        return "unknown";
    }

    scene_transition_presentation_t make_transition_presentation(const scene_runtime_snapshot_t& snapshot,
                                                                 const uint32_t overlay_color_abgr) noexcept
    {
        if (!snapshot.is_transitioning())
            return {};

        scene_transition_presentation_t presentation{
            .visible = true,
            .show_loading_text = false,
            .overlay_opacity = 0.7f,
            .progress = std::clamp(snapshot.transition_progress, 0.f, 1.f),
            .overlay_color_abgr = overlay_color_abgr,
            .phase_label = to_string(snapshot.transition_phase)
        };

        switch (snapshot.transition_phase)
        {
            case scene_transition_phase_t::preparing:
                presentation.overlay_opacity = 0.35f + (0.4f * presentation.progress);
                break;
            case scene_transition_phase_t::loading:
                presentation.overlay_opacity = 0.78f;
                break;
            case scene_transition_phase_t::activating:
                presentation.overlay_opacity = 0.9f;
                break;
            case scene_transition_phase_t::finalizing:
                presentation.overlay_opacity = 0.45f;
                break;
            case scene_transition_phase_t::none:
                presentation.visible = false;
                presentation.show_loading_text = false;
                presentation.overlay_opacity = 0.f;
                break;
        }

        if (!presentation.visible)
            presentation.overlay_color_abgr = 0x00000000u;

        return presentation;
    }

    scene_transition_overlay_options_t make_default_transition_overlay_options() noexcept
    {
        return scene_transition_overlay_options_t{};
    }

    scene_camera_options_t make_default_scene_camera_options() noexcept
    {
        return scene_camera_options_t{};
    }

    scene_camera_options_t resolve_scene_camera_options(const scene_camera_options_t& defaults,
                                                        const scene_camera_override_t& override) noexcept
    {
        scene_camera_options_t resolved{ defaults };

        if (override.projection_mode.has_value())
            resolved.projection_mode = *override.projection_mode;
        if (override.bounds_mode.has_value())
            resolved.bounds_mode = *override.bounds_mode;
        if (override.zoom.has_value())
            resolved.zoom = *override.zoom;
        if (override.follow_mode.has_value())
            resolved.follow_mode = *override.follow_mode;
        if (override.initial_target_policy.has_value())
            resolved.initial_target_policy = *override.initial_target_policy;
        if (override.dead_zone_size_world.has_value())
            resolved.dead_zone_size_world = *override.dead_zone_size_world;
        if (override.follow_smoothing.has_value())
            resolved.follow_smoothing = *override.follow_smoothing;

        resolved.zoom = std::max(0.001f, resolved.zoom);
        resolved.dead_zone_size_world = {
            std::max(0.f, resolved.dead_zone_size_world.x),
            std::max(0.f, resolved.dead_zone_size_world.y)
        };
        resolved.follow_smoothing = std::max(0.f, resolved.follow_smoothing);

        return resolved;
    }

    scene_transition_overlay_options_t resolve_transition_overlay_options(
        const scene_transition_overlay_options_t& defaults,
        const scene_transition_overlay_override_t& override) noexcept
    {
        scene_transition_overlay_options_t resolved{ defaults };

        switch (override.style)
        {
            case scene_transition_overlay_style_t::inherit:
                break;
            case scene_transition_overlay_style_t::none:
                resolved.enabled = false;
                break;
            case scene_transition_overlay_style_t::fade:
                resolved.enabled = true;
                resolved.style = scene_transition_overlay_style_t::fade;
                break;
            case scene_transition_overlay_style_t::loading_screen:
                resolved.enabled = true;
                resolved.style = scene_transition_overlay_style_t::loading_screen;
                break;
            case scene_transition_overlay_style_t::wipe:
                resolved.enabled = true;
                resolved.style = scene_transition_overlay_style_t::wipe;
                break;
        }

        if (override.wipe_direction.has_value())
            resolved.wipe_direction = *override.wipe_direction;
        if (override.overlay_color_abgr.has_value())
            resolved.overlay_color_abgr = *override.overlay_color_abgr;
        if (override.fade_out_to_black_seconds.has_value())
            resolved.fade_out_to_black_seconds = *override.fade_out_to_black_seconds;
        if (override.minimum_opaque_hold_seconds.has_value())
            resolved.minimum_opaque_hold_seconds = *override.minimum_opaque_hold_seconds;
        if (override.fade_in_from_black_seconds.has_value())
            resolved.fade_in_from_black_seconds = *override.fade_in_from_black_seconds;
        if (override.show_loading_text.has_value())
            resolved.show_loading_text = *override.show_loading_text;
        if (override.loading_text_color_abgr.has_value())
            resolved.loading_text_color_abgr = *override.loading_text_color_abgr;
        if (override.loading_subtext_color_abgr.has_value())
            resolved.loading_subtext_color_abgr = *override.loading_subtext_color_abgr;
        if (override.show_progress_text.has_value())
            resolved.show_progress_text = *override.show_progress_text;
        if (override.loading_title_text.has_value())
            resolved.loading_title_text = *override.loading_title_text;
        if (override.loading_subtitle_text.has_value())
            resolved.loading_subtitle_text = *override.loading_subtitle_text;

        resolved.fade_out_to_black_seconds = std::max(0.f, resolved.fade_out_to_black_seconds);
        resolved.minimum_opaque_hold_seconds = std::max(0.f, resolved.minimum_opaque_hold_seconds);
        resolved.fade_in_from_black_seconds = std::max(0.f, resolved.fade_in_from_black_seconds);

        if (resolved.style == scene_transition_overlay_style_t::loading_screen)
            resolved.show_loading_text = true;

        return resolved;
    }

    world::world_object_t* scene_runtime_context_t::find_object_by_id(const world::world_object_id_t object_id) const noexcept
    {
        for (world::world_object_t& object : world.objects())
        {
            if (object.id == object_id)
                return &object;
        }

        return nullptr;
    }

    world::world_object_t* scene_runtime_context_t::player() const noexcept
    {
        if (!scene_record)
            return nullptr;

        return world.find_object_by_name(scene_record->scene.player_name);
    }

    const world::world_object_t* scene_runtime_context_t::spawn_object() const noexcept
    {
        return find_spawn_marker(world, spawn_marker);
    }

    bool scene_runtime_t::load(core::game_context_t& game,
                               const std::string_view scene_id,
                               const scene_load_options_t& options)
    {
        if (!request_load(game, scene_id, options))
            return false;

        while (has_pending_scene())
            (void)update(game);

        return _last_scene_change_succeeded;
    }

    bool scene_runtime_t::request_load(core::game_context_t& game,
                                       const std::string_view scene_id,
                                       const scene_load_options_t& options)
    {
        if (!can_accept_scene_change_request())
            return false;

        const assets::scene_asset_record_t* scene_record{ game.assets.scenes().registry().find(scene_id) };
        if (!scene_record)
            return false;

        const std::string resolved_spawn_marker{
            options.spawn_marker_override.empty()
                ? std::string{ scene_record->scene.player_spawn_marker }
                : std::string{ options.spawn_marker_override }
        };
        const bool had_scene_loaded{ has_scene_loaded() };
        const scene_runtime_context_t previous_context{
            make_context(game)
        };

        _pending_options = options;
        _pending_options.spawn_marker_override = {};
        _active_transition_overlay_options = resolve_transition_overlay_options(_transition_overlay_options,
                                                                                options.transition_overlay);
        LOG_CORE_INFO("Scene transition requested: current='{}', target='{}', spawn='{}', overlay='{}'",
                      _current_scene_id.empty() ? "<none>" : _current_scene_id,
                      scene_id,
                      resolved_spawn_marker,
                      to_string(_active_transition_overlay_options.style));
        _pending_load_task.emplace(scene_id, resolved_spawn_marker);
        _last_scene_change_succeeded = false;
        begin_scene_change(*scene_record, scene_id, resolved_spawn_marker);

        if (options.listener)
            options.listener->before_scene_change(game,
                                                 had_scene_loaded ? &previous_context : nullptr,
                                                 scene_id,
                                                 resolved_spawn_marker);

        return true;
    }

    bool scene_runtime_t::request_transition(core::game_context_t& game,
                                             const scene_transition_request_t& request,
                                             const scene_load_options_t& options)
    {
        scene_load_options_t transition_options{ options };
        transition_options.spawn_marker_override = request.marker_name;
        return request_load(game, request.scene_id, transition_options);
    }

    bool scene_runtime_t::update(core::game_context_t& game)
    {
        if (!_pending_load_task.has_value())
            return false;

        if (!_pending_load_task->advance(game.assets))
        {
            fail_scene_change();
            return true;
        }

        _transition_phase = _pending_load_task->is_background_preparing()
                                ? scene_transition_phase_t::preparing
                                : scene_transition_phase_t::loading;

        if (!_pending_load_task->is_ready_to_activate())
            return true;

        if (!can_activate_scene_change())
            return true;

        world::world_t staged_world{ _pending_load_task->take_world() };
        if (_pending_options.validate_loaded_scene &&
            !_pending_options.validate_loaded_scene(game.assets, staged_world, _pending_scene_id))
        {
            fail_scene_change();
            return true;
        }

        _transition_phase = scene_transition_phase_t::activating;
        game.world = std::move(staged_world);

        _player_controller = _pending_options.player_controller;
        _interaction_controller = _pending_options.interaction_controller;
        _current_scene_record = _pending_scene_record;
        _current_scene_id = _pending_scene_id;
        _current_spawn_marker = _pending_spawn_marker;

        bind_runtime_objects(game, _current_scene_record->scene);

        if (_pending_options.apply_camera_defaults)
        {
            apply_camera_defaults(game, _current_scene_record->scene, _pending_options.camera_override);
            center_camera_on_initial_target(game);
        }

        if (_pending_options.apply_scene_music)
            refresh_scene_music(_current_scene_record->scene);

        _transition_phase = scene_transition_phase_t::finalizing;
        if (_pending_options.listener)
        {
            const scene_runtime_context_t current_context{ make_context(game) };
            _pending_options.listener->after_scene_change(game, current_context);
        }

        complete_scene_change();
        return true;
    }

    bool scene_runtime_t::transition(core::game_context_t& game,
                                     const scene_transition_request_t& request,
                                     const scene_load_options_t& options)
    {
        if (!request_transition(game, request, options))
            return false;

        while (has_pending_scene())
            (void)update(game);

        return _last_scene_change_succeeded;
    }

    void scene_runtime_t::update_camera(core::game_context_t& game, const float delta_time) noexcept
    {
        if (_active_camera_options.follow_mode != assets::scene_camera_follow_mode_t::player || !_player_controller)
            return;

        if (const world::world_object_t* player{ _player_controller->controlled_object() };
            player && player->transform)
        {
            const chlm::float2 current_center{ game.view.center_world_position(game.world) };
            const chlm::float2 target_position{ player->transform->position };
            const chlm::float2 half_dead_zone{
                _active_camera_options.dead_zone_size_world.x * 0.5f,
                _active_camera_options.dead_zone_size_world.y * 0.5f
            };

            chlm::float2 desired_center{
                apply_dead_zone_axis(current_center.x, target_position.x, half_dead_zone.x),
                apply_dead_zone_axis(current_center.y, target_position.y, half_dead_zone.y)
            };

            if (_active_camera_options.follow_smoothing > 0.f && delta_time > 0.f)
            {
                const float alpha{ 1.f - std::exp(-_active_camera_options.follow_smoothing * delta_time) };
                desired_center = {
                    current_center.x + ((desired_center.x - current_center.x) * alpha),
                    current_center.y + ((desired_center.y - current_center.y) * alpha)
                };
            }

            game.view.set_center_world_position(game.world, desired_center);
        }
    }

    void scene_runtime_t::bind_runtime_objects(core::game_context_t& game, const assets::scene_asset_t& scene) noexcept
    {
        if (_player_controller)
            _player_controller->set_controlled_object(find_scene_player(game.world, scene));

        if (_interaction_controller)
            _interaction_controller->set_actor(_player_controller ? _player_controller->controlled_object() : nullptr);
    }

    void scene_runtime_t::apply_camera_defaults(core::game_context_t& game,
                                                const assets::scene_asset_t& scene,
                                                const scene_camera_override_t& override) noexcept
    {
        _active_camera_options = resolve_scene_camera_options(
            resolve_scene_camera_options(_camera_options, make_scene_camera_override(scene.camera)),
            override);
        game.view.set_zoom(_active_camera_options.zoom);
    }

    void scene_runtime_t::center_camera_on_initial_target(core::game_context_t& game) noexcept
    {
        if (_active_camera_options.initial_target_policy == assets::scene_camera_initial_target_policy_t::spawn_marker)
        {
            if (const world::world_object_t* marker{ find_spawn_marker(game.world, _current_spawn_marker) };
                marker && marker->transform)
            {
                game.view.set_center_world_position(game.world, marker->transform->position);
                return;
            }
        }

        if (_player_controller)
        {
            if (const world::world_object_t* player{ _player_controller->controlled_object() };
                player && player->transform)
            {
                game.view.set_center_world_position(game.world, player->transform->position);
            }
        }
    }

    void scene_runtime_t::refresh_scene_music(const assets::scene_asset_t& scene) noexcept
    {
        if (_music_handle.is_valid())
            audio::stop(_music_handle);

        _music_handle = audio::voice_handle_t::invalid();
        if (!scene.initial_music_id.empty())
            _music_handle = audio::play(scene.initial_music_id);
    }

    scene_runtime_context_t scene_runtime_t::make_context(core::game_context_t& game) const noexcept
    {
        return scene_runtime_context_t{
            .world = game.world,
            .assets = game.assets,
            .view = game.view,
            .scene_record = _current_scene_record,
            .scene_id = _current_scene_id,
            .spawn_marker = _current_spawn_marker
        };
    }

    scene_runtime_snapshot_t scene_runtime_t::snapshot() const noexcept
    {
        const size_t completed_steps{
            _pending_load_task ? _pending_load_task->completed_steps() : 0u
        };
        const size_t total_steps{
            _pending_load_task ? _pending_load_task->total_steps() : 0u
        };
        return scene_runtime_snapshot_t{
            .runtime_state = _runtime_state,
            .transition_phase = _transition_phase,
            .active_scene_record = _current_scene_record,
            .pending_scene_record = _pending_scene_record,
            .active_scene_id = _current_scene_id,
            .active_spawn_marker = _current_spawn_marker,
            .pending_scene_id = _pending_scene_id,
            .pending_spawn_marker = _pending_spawn_marker,
            .transition_completed_steps = completed_steps,
            .transition_total_steps = total_steps,
            .transition_progress = total_steps > 0u
                                       ? static_cast<float>(completed_steps) / static_cast<float>(total_steps)
                                       : 0.f
        };
    }

    void scene_runtime_t::set_default_camera_options(scene_camera_options_t options) noexcept
    {
        _camera_options = resolve_scene_camera_options(
            _engine_camera_options,
            scene_camera_override_t{
                .projection_mode = options.projection_mode,
                .bounds_mode = options.bounds_mode,
                .zoom = options.zoom,
                .follow_mode = options.follow_mode,
                .initial_target_policy = options.initial_target_policy,
                .dead_zone_size_world = options.dead_zone_size_world,
                .follow_smoothing = options.follow_smoothing
            });
    }

    void scene_runtime_t::set_default_camera_override(scene_camera_override_t override) noexcept
    {
        _camera_options = resolve_scene_camera_options(_engine_camera_options, override);
    }

    void scene_runtime_t::set_default_transition_overlay_options(scene_transition_overlay_options_t options) noexcept
    {
        _transition_overlay_options = resolve_transition_overlay_options(
            _engine_transition_overlay_options,
            scene_transition_overlay_override_t{
                .style = options.enabled ? options.style : scene_transition_overlay_style_t::none,
                .wipe_direction = options.wipe_direction,
                .overlay_color_abgr = options.overlay_color_abgr,
                .fade_out_to_black_seconds = options.fade_out_to_black_seconds,
                .minimum_opaque_hold_seconds = options.minimum_opaque_hold_seconds,
                .fade_in_from_black_seconds = options.fade_in_from_black_seconds,
                .show_loading_text = options.show_loading_text,
                .loading_text_color_abgr = options.loading_text_color_abgr,
                .loading_subtext_color_abgr = options.loading_subtext_color_abgr,
                .show_progress_text = options.show_progress_text,
                .loading_title_text = options.loading_title_text,
                .loading_subtitle_text = options.loading_subtitle_text
            }
        );
        if (!_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
        }
    }

    void scene_runtime_t::set_default_transition_overlay_override(scene_transition_overlay_override_t override) noexcept
    {
        _transition_overlay_options = resolve_transition_overlay_options(_engine_transition_overlay_options, override);
        if (!_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
        }
    }

    void scene_runtime_t::advance_transition_overlay(const float delta_time) noexcept
    {
        const float dt{ std::max(0.f, delta_time) };
        if (!_active_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
            if (_transition_diagnostics_hold_remaining_seconds > 0.f)
            {
                _transition_diagnostics_hold_remaining_seconds = std::max(0.f,
                                                                          _transition_diagnostics_hold_remaining_seconds -
                                                                              dt);
            }
            return;
        }

        const bool transitioning{ is_transitioning() };

        if (transitioning || _transition_overlay_stage != transition_overlay_stage_t::hidden)
        {
            capture_recent_transition_diagnostics();
            _transition_diagnostics_hold_remaining_seconds = k_transition_diagnostics_linger_seconds;
        }
        else if (_transition_diagnostics_hold_remaining_seconds > 0.f)
        {
            _transition_diagnostics_hold_remaining_seconds = std::max(0.f,
                                                                      _transition_diagnostics_hold_remaining_seconds -
                                                                          dt);
        }

        if (transitioning)
        {
            if (_transition_overlay_stage == transition_overlay_stage_t::hidden ||
                _transition_overlay_stage == transition_overlay_stage_t::fading_in_from_black)
            {
                _transition_overlay_stage = transition_overlay_stage_t::fading_out_to_black;
                _transition_overlay_hold_elapsed_seconds = 0.f;
            }
        }

        switch (_transition_overlay_stage)
        {
            case transition_overlay_stage_t::hidden:
                _transition_overlay_opacity = 0.f;
                break;
            case transition_overlay_stage_t::fading_out_to_black:
            {
                if (_active_transition_overlay_options.fade_out_to_black_seconds <= 0.f)
                    _transition_overlay_opacity = 1.f;
                else
                    _transition_overlay_opacity = std::min(
                        1.f,
                        _transition_overlay_opacity +
                            (dt / _active_transition_overlay_options.fade_out_to_black_seconds)
                    );

                if (_transition_overlay_opacity >= 0.999f)
                {
                    _transition_overlay_opacity = 1.f;
                    _transition_overlay_stage = transition_overlay_stage_t::holding_opaque;
                    _transition_overlay_hold_elapsed_seconds = 0.f;
                }
                break;
            }
            case transition_overlay_stage_t::holding_opaque:
                _transition_overlay_opacity = 1.f;
                if (!transitioning)
                {
                    if (_startup_overlay_waiting_for_first_present)
                        break;

                    _transition_overlay_hold_elapsed_seconds += dt;
                    if (_transition_overlay_hold_elapsed_seconds >= _active_transition_overlay_options.minimum_opaque_hold_seconds)
                        _transition_overlay_stage = transition_overlay_stage_t::fading_in_from_black;
                }
                break;
            case transition_overlay_stage_t::fading_in_from_black:
            {
                if (_active_transition_overlay_options.fade_in_from_black_seconds <= 0.f)
                    _transition_overlay_opacity = 0.f;
                else
                    _transition_overlay_opacity = std::max(
                        0.f,
                        _transition_overlay_opacity -
                            (dt / _active_transition_overlay_options.fade_in_from_black_seconds)
                    );

                if (_transition_overlay_opacity <= 0.001f)
                {
                    _transition_overlay_opacity = 0.f;
                    _transition_overlay_stage = transition_overlay_stage_t::hidden;
                    _transition_overlay_hold_elapsed_seconds = 0.f;
                }
                break;
            }
        }
    }

    void scene_runtime_t::render_transition_overlay(core::game_context_t& game) noexcept
    {
        if (!_active_transition_overlay_options.enabled || _transition_overlay_opacity <= 0.001f)
        {
            game.view.clear_fullscreen_overlay();
            render_transition_diagnostics(game);
            return;
        }

        if (_startup_overlay_waiting_for_first_present && !is_transitioning())
            _startup_overlay_waiting_for_first_present = false;

        const uint32_t overlay_alpha{
            static_cast<uint32_t>(std::round(std::clamp(_transition_overlay_opacity, 0.f, 1.f) * 255.f)) & 0xFFu
        };
        const uint32_t overlay_color{
            (_active_transition_overlay_options.overlay_color_abgr & 0x00FFFFFFu) | (overlay_alpha << 24u)
        };

        if (_active_transition_overlay_options.style == scene_transition_overlay_style_t::wipe)
        {
            game.view.clear_fullscreen_overlay();
            const float viewport_width{ static_cast<float>(window::get_width()) };
            const float viewport_height{ static_cast<float>(window::get_height()) };
            const float coverage{ std::clamp(_transition_overlay_opacity, 0.f, 1.f) };
            switch (_active_transition_overlay_options.wipe_direction)
            {
                case scene_transition_wipe_direction_t::left_to_right:
                {
                    const float covered_width{ coverage * viewport_width };
                    if (covered_width > 0.001f && viewport_height > 0.001f)
                        game.view.draw_overlay_solid_quad(0.f, 0.f, covered_width, viewport_height, overlay_color);
                    break;
                }
                case scene_transition_wipe_direction_t::right_to_left:
                {
                    const float covered_width{ coverage * viewport_width };
                    if (covered_width > 0.001f && viewport_height > 0.001f)
                        game.view.draw_overlay_solid_quad(viewport_width - covered_width,
                                                          0.f,
                                                          covered_width,
                                                          viewport_height,
                                                          overlay_color);
                    break;
                }
                case scene_transition_wipe_direction_t::top_to_bottom:
                {
                    const float covered_height{ coverage * viewport_height };
                    if (viewport_width > 0.001f && covered_height > 0.001f)
                        game.view.draw_overlay_solid_quad(0.f, 0.f, viewport_width, covered_height, overlay_color);
                    break;
                }
                case scene_transition_wipe_direction_t::bottom_to_top:
                {
                    const float covered_height{ coverage * viewport_height };
                    if (viewport_width > 0.001f && covered_height > 0.001f)
                        game.view.draw_overlay_solid_quad(0.f,
                                                          viewport_height - covered_height,
                                                          viewport_width,
                                                          covered_height,
                                                          overlay_color);
                    break;
                }
            }
        }
        else
        {
            game.view.set_fullscreen_overlay_color(overlay_color);
        }

        if (!_active_transition_overlay_options.show_loading_text ||
            _active_transition_overlay_options.style != scene_transition_overlay_style_t::loading_screen)
        {
            render_transition_diagnostics(game);
            return;
        }

        const scene_runtime_snapshot_t state_snapshot{ snapshot() };
        const float viewport_width{ static_cast<float>(window::get_width()) };
        const float viewport_height{ static_cast<float>(window::get_height()) };
        const float center_x{ viewport_width * 0.5f };
        const float center_y{ viewport_height * 0.46f };
        const std::string_view title{
            _active_transition_overlay_options.loading_title_text.empty()
                ? std::string_view{ "Loading..." }
                : std::string_view{ _active_transition_overlay_options.loading_title_text }
        };
        const std::string_view subtitle{
            _active_transition_overlay_options.loading_subtitle_text.empty()
                ? (_pending_scene_id.empty() ? std::string_view{} : std::string_view{ _pending_scene_id })
                : std::string_view{ _active_transition_overlay_options.loading_subtitle_text }
        };

        debug::text_colored(std::max(16.f, center_x - 96.f),
                            std::max(16.f, center_y - 18.f),
                            _active_transition_overlay_options.loading_text_color_abgr,
                            "%.*s",
                            static_cast<int>(title.size()),
                            title.data());

        if (!subtitle.empty())
        {
            debug::text_colored(std::max(16.f, center_x - 164.f),
                                std::max(16.f, center_y + 18.f),
                                _active_transition_overlay_options.loading_subtext_color_abgr,
                                "%.*s",
                                static_cast<int>(subtitle.size()),
                                subtitle.data());
        }

        if (_active_transition_overlay_options.show_progress_text)
        {
            debug::text_colored(std::max(16.f, center_x - 54.f),
                                std::max(16.f, center_y + 48.f),
                                _active_transition_overlay_options.loading_subtext_color_abgr,
                                "%.0f%%",
                                std::round(std::clamp(state_snapshot.transition_progress, 0.f, 1.f) * 100.f));
        }

        render_transition_diagnostics(game);
    }

    void scene_runtime_t::capture_recent_transition_diagnostics() noexcept
    {
        const scene_runtime_snapshot_t state_snapshot{ snapshot() };
        _recent_transition_diagnostics.runtime_state = state_snapshot.runtime_state;
        _recent_transition_diagnostics.transition_phase = state_snapshot.transition_phase;
        _recent_transition_diagnostics.overlay_style = _active_transition_overlay_options.style;
        _recent_transition_diagnostics.wipe_direction = _active_transition_overlay_options.wipe_direction;
        _recent_transition_diagnostics.active_scene_id = std::string{ state_snapshot.active_scene_id };
        _recent_transition_diagnostics.pending_scene_id = std::string{ state_snapshot.pending_scene_id };
        _recent_transition_diagnostics.pending_spawn_marker = std::string{ state_snapshot.pending_spawn_marker };
        _recent_transition_diagnostics.transition_progress = state_snapshot.transition_progress;
        _recent_transition_diagnostics.startup_waiting_for_first_present = _startup_overlay_waiting_for_first_present;
    }

    void scene_runtime_t::render_transition_diagnostics(core::game_context_t& game) noexcept
    {
        if (!is_transitioning() && _transition_diagnostics_hold_remaining_seconds <= 0.f)
            return;

        constexpr float panel_y{ 12.f };
        constexpr float panel_width{ 340.f };
        constexpr float panel_height{ 82.f };
        constexpr float line_step{ 16.f };
        const float panel_x{ std::max(12.f, static_cast<float>(window::get_width()) - panel_width - 12.f) };
        const float text_x{ panel_x + 10.f };

        game.view.draw_overlay_solid_quad(panel_x, panel_y, panel_width, panel_height, 0xA0101010u);

        const std::string_view current_scene{
            _recent_transition_diagnostics.active_scene_id.empty()
                ? std::string_view{ "<none>" }
                : std::string_view{ _recent_transition_diagnostics.active_scene_id }
        };
        const std::string_view pending_scene{
            _recent_transition_diagnostics.pending_scene_id.empty()
                ? std::string_view{ "<none>" }
                : std::string_view{ _recent_transition_diagnostics.pending_scene_id }
        };

        debug::text_colored_sized(text_x,
                                  panel_y + 8.f,
                                  k_transition_diagnostics_font_size,
                                  0xFFE9E2D6u,
                                  "scene runtime");
        debug::text_colored_sized(text_x,
                                  panel_y + 8.f + line_step,
                                  k_transition_diagnostics_font_size,
                                  0xFFF5F1E8u,
                                  "current: %.*s  target: %.*s",
                                  static_cast<int>(current_scene.size()),
                                  current_scene.data(),
                                  static_cast<int>(pending_scene.size()),
                                  pending_scene.data());
        debug::text_colored_sized(text_x,
                                  panel_y + 8.f + (line_step * 2.f),
                                  k_transition_diagnostics_font_size,
                                  0xFFD7CDBEu,
                                  "phase: %s  progress: %.0f%%",
                                  to_string(_recent_transition_diagnostics.transition_phase).data(),
                                  std::round(std::clamp(_recent_transition_diagnostics.transition_progress, 0.f, 1.f) * 100.f));

        if (_recent_transition_diagnostics.overlay_style == scene_transition_overlay_style_t::wipe)
        {
            debug::text_colored_sized(text_x,
                                      panel_y + 8.f + (line_step * 3.f),
                                      k_transition_diagnostics_font_size,
                                      0xFFD7CDBEu,
                                      "overlay: %s %s",
                                      to_string(_recent_transition_diagnostics.overlay_style).data(),
                                      to_string(_recent_transition_diagnostics.wipe_direction).data());
        }
        else
        {
            debug::text_colored_sized(text_x,
                                      panel_y + 8.f + (line_step * 3.f),
                                      k_transition_diagnostics_font_size,
                                      0xFFD7CDBEu,
                                      "overlay: %s%s",
                                      to_string(_recent_transition_diagnostics.overlay_style).data(),
                                      _recent_transition_diagnostics.startup_waiting_for_first_present ? " (boot wait)" : "");
        }
    }

    void scene_runtime_t::begin_scene_change(const assets::scene_asset_record_t& scene_record,
                                             const std::string_view scene_id,
                                             const std::string_view spawn_marker) noexcept
    {
        const bool boot_loading{ !has_scene_loaded() };
        _pending_scene_record = &scene_record;
        _pending_scene_id = std::string{ scene_id };
        _pending_spawn_marker = std::string{ spawn_marker };
        _runtime_state = has_scene_loaded()
                             ? scene_runtime_state_t::transitioning
                             : scene_runtime_state_t::loading;
        _transition_phase = scene_transition_phase_t::preparing;

        if (!_active_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
            return;
        }

        if (boot_loading)
        {
            // Startup should begin under a fully black presentation instead of waiting
            // for the normal fade-to-black ramp to catch up on later ticks.
            _transition_overlay_stage = transition_overlay_stage_t::holding_opaque;
            _transition_overlay_opacity = 1.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
            _startup_overlay_waiting_for_first_present = true;
            return;
        }

        _transition_overlay_stage = transition_overlay_stage_t::hidden;
        _transition_overlay_opacity = 0.f;
        _transition_overlay_hold_elapsed_seconds = 0.f;
        _startup_overlay_waiting_for_first_present = false;
    }

    bool scene_runtime_t::can_activate_scene_change() const noexcept
    {
        if (!_active_transition_overlay_options.enabled)
            return true;

        switch (_transition_overlay_stage)
        {
            case transition_overlay_stage_t::holding_opaque:
                return _transition_overlay_opacity >= 0.999f;
            case transition_overlay_stage_t::hidden:
            case transition_overlay_stage_t::fading_out_to_black:
            case transition_overlay_stage_t::fading_in_from_black:
                return false;
        }

        return false;
    }

    bool scene_runtime_t::can_accept_scene_change_request() const noexcept
    {
        if (has_pending_scene())
            return false;

        return _transition_overlay_stage == transition_overlay_stage_t::hidden;
    }

    void scene_runtime_t::fail_scene_change() noexcept
    {
        capture_recent_transition_diagnostics();
        LOG_CORE_WARN("Scene transition failed: current='{}', pending='{}', phase='{}'",
                      _current_scene_id.empty() ? "<none>" : _current_scene_id,
                      _pending_scene_id.empty() ? "<none>" : _pending_scene_id,
                      to_string(_transition_phase));
        _pending_load_task.reset();
        _pending_scene_record = nullptr;
        _pending_scene_id.clear();
        _pending_spawn_marker.clear();
        _pending_options = {};
        _transition_phase = scene_transition_phase_t::none;
        _runtime_state = has_scene_loaded()
                             ? scene_runtime_state_t::active
                             : scene_runtime_state_t::idle;
        _last_scene_change_succeeded = false;
        _transition_diagnostics_hold_remaining_seconds = k_transition_diagnostics_linger_seconds;
    }

    void scene_runtime_t::complete_scene_change() noexcept
    {
        capture_recent_transition_diagnostics();
        LOG_CORE_INFO("Scene transition complete: current='{}', spawn='{}'",
                      _current_scene_id.empty() ? "<none>" : _current_scene_id,
                      _current_spawn_marker.empty() ? "<none>" : _current_spawn_marker);
        _pending_load_task.reset();
        _pending_scene_record = nullptr;
        _pending_scene_id.clear();
        _pending_spawn_marker.clear();
        _pending_options = {};
        _transition_phase = scene_transition_phase_t::none;
        _runtime_state = has_scene_loaded()
                             ? scene_runtime_state_t::active
                             : scene_runtime_state_t::idle;
        _last_scene_change_succeeded = true;
        _transition_diagnostics_hold_remaining_seconds = k_transition_diagnostics_linger_seconds;
    }

    bool load(core::game_context_t& game,
              scene_runtime_t& runtime,
              const std::string_view scene_id,
              const scene_load_options_t& options)
    {
        return runtime.load(game, scene_id, options);
    }

    bool transition(core::game_context_t& game,
                    scene_runtime_t& runtime,
                    const scene_transition_request_t& request,
                    const scene_load_options_t& options)
    {
        return runtime.transition(game, request, options);
    }
} // namespace carrot::scene
