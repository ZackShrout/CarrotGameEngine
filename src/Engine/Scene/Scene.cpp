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
#include "World/AuthoredInteractions.h"
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

        [[nodiscard]] scene_runtime_object_interaction_kind_t to_runtime_interaction_kind(
            const world::authored::interaction_kind_t kind) noexcept
        {
            switch (kind)
            {
                case world::authored::interaction_kind_t::sign:
                    return scene_runtime_object_interaction_kind_t::sign;
                case world::authored::interaction_kind_t::door:
                    return scene_runtime_object_interaction_kind_t::door;
                case world::authored::interaction_kind_t::container:
                    return scene_runtime_object_interaction_kind_t::container;
                case world::authored::interaction_kind_t::none:
                default:
                    return scene_runtime_object_interaction_kind_t::none;
            }
        }

        [[nodiscard]] std::string_view to_string(const world::facing_direction_t direction) noexcept
        {
            switch (direction)
            {
                case world::facing_direction_t::down: return "down";
                case world::facing_direction_t::up: return "up";
                case world::facing_direction_t::left: return "left";
                case world::facing_direction_t::right: return "right";
            }

            return "unknown";
        }

        [[nodiscard]] scene_runtime_object_summary_t summarize_world_object(const world::world_object_t& object)
        {
            scene_runtime_object_summary_t summary{
                .id = object.id,
                .name = object.name,
                .type = object.type,
                .property_count = static_cast<uint32_t>(object.properties.size())
            };

            if (object.source)
            {
                summary.has_source = true;
                summary.source = scene_runtime_object_source_summary_t{
                    .tilemap_logical_id = object.source->tilemap_logical_id,
                    .layer_name = object.source->layer_name,
                    .object_id = object.source->object_id,
                    .object_name = object.source->object_name
                };
            }

            if (object.transform)
            {
                summary.has_transform = true;
                summary.transform = scene_runtime_object_transform_summary_t{
                    .position = object.transform->position,
                    .scale = object.transform->scale,
                    .rotation = object.transform->rotation
                };
            }

            if (object.collision)
            {
                summary.has_collision = true;
                summary.collision.half_extents = object.collision->half_extents;
                summary.collision.offset = object.collision->offset;
                if (object.collision->debug_display)
                {
                    summary.collision.has_debug_display = true;
                    summary.collision.debug_filled = object.collision->debug_display->filled;
                    summary.collision.debug_outline_thickness = object.collision->debug_display->outline_thickness;
                    summary.collision.debug_color = object.collision->debug_display->color;
                }
            }

            if (object.trigger)
            {
                summary.has_trigger = true;
                summary.trigger.trigger_id = object.trigger->trigger_id;
                summary.trigger.trigger_kind = object.trigger->trigger_kind;
            }

            if (object.sprite)
            {
                summary.has_sprite = true;
                if (object.sprite->sprite)
                    summary.sprite.texture_id = std::string{ object.sprite->sprite->sprite().texture_id() };
                if (object.sprite->frame)
                    summary.sprite.frame_name = object.sprite->frame->name;
                summary.sprite.use_size_override = object.sprite->use_size_override;
                summary.sprite.size_override_world = object.sprite->size_override_world;
                summary.sprite.use_custom_pivot = object.sprite->use_custom_pivot;
                summary.sprite.pivot = object.sprite->pivot;
                summary.sprite.flip_x = object.sprite->flip_x;
                summary.sprite.flip_y = object.sprite->flip_y;
                summary.sprite.layer = object.sprite->layer;
                summary.sprite.order_mode = object.sprite->order_mode;
                summary.sprite.order_in_layer = object.sprite->order_in_layer;
                summary.sprite.sort_reference_y = object.sprite->sort_reference_y;
                summary.sprite.color = object.sprite->color;
            }

            if (object.sprite_animator)
            {
                summary.has_sprite_animator = true;
                if (const assets::sprite_animation_t* animation{ object.sprite_animator->animator.current_animation() })
                    summary.sprite_animator.current_animation_name = animation->name;
                if (const assets::sprite_frame_t* frame{ object.sprite_animator->animator.current_frame() })
                    summary.sprite_animator.current_frame_name = frame->name;
                summary.sprite_animator.is_playing = object.sprite_animator->animator.is_playing();
                summary.sprite_animator.is_paused = object.sprite_animator->animator.is_paused();
                summary.sprite_animator.is_finished = object.sprite_animator->animator.is_finished();
            }

            if (object.tile_object)
            {
                summary.has_tile_object = true;
                if (object.tile_object->tilemap && object.tile_object->tilemap->record())
                    summary.tile_object.tilemap_logical_id = object.tile_object->tilemap->record()->logical_id;
                summary.tile_object.gid = object.tile_object->gid;
                summary.tile_object.size_source_px = object.tile_object->size_source_px;
                summary.tile_object.layer = object.tile_object->layer;
                summary.tile_object.order_mode = object.tile_object->order_mode;
                summary.tile_object.order_in_layer = object.tile_object->order_in_layer;
                summary.tile_object.sort_reference_y = object.tile_object->sort_reference_y;
                summary.tile_object.color = object.tile_object->color;
            }

            if (object.tilemap)
            {
                summary.has_tilemap = true;
                if (object.tilemap->tilemap && object.tilemap->tilemap->record())
                    summary.tilemap.tilemap_logical_id = object.tilemap->tilemap->record()->logical_id;
                summary.tilemap.include_object_layers = object.tilemap->include_object_layers;
                summary.tilemap.layer = object.tilemap->layer;
                summary.tilemap.order_mode = object.tilemap->order_mode;
                summary.tilemap.order_in_layer = object.tilemap->order_in_layer;
                summary.tilemap.sort_reference_y = object.tilemap->sort_reference_y;
                summary.tilemap.color = object.tilemap->color;
            }

            if (object.visibility_region)
            {
                summary.has_visibility_region = true;
                summary.visibility_region.size_world = object.visibility_region->size_world;
                summary.visibility_region.tag = object.visibility_region->tag;
            }

            if (const auto sign{ world::authored::as_sign(object) })
            {
                summary.has_interaction = true;
                summary.interaction.kind = scene_runtime_object_interaction_kind_t::sign;
                summary.interaction.message_id = sign->message_id;
            }
            else if (const auto door{ world::authored::as_door(object) })
            {
                summary.has_interaction = true;
                summary.interaction.kind = scene_runtime_object_interaction_kind_t::door;
                summary.interaction.target_scene = door->target_scene;
                summary.interaction.target_map = door->target_map;
                summary.interaction.target_marker = door->target_marker;
            }
            else if (const auto container{ world::authored::as_container(object) })
            {
                summary.has_interaction = true;
                summary.interaction.kind = scene_runtime_object_interaction_kind_t::container;
                summary.interaction.loot_table = container->loot_table;
            }
            else if (const auto trigger{ world::authored::as_trigger(object) })
            {
                summary.has_interaction = true;
                summary.interaction.kind = scene_runtime_object_interaction_kind_t::trigger;
                summary.interaction.trigger_id = trigger->trigger_id;
                summary.interaction.trigger_kind = trigger->trigger_kind;
            }
            else
            {
                summary.interaction.kind = to_runtime_interaction_kind(world::authored::interaction_kind_for(object));
                summary.has_interaction = summary.interaction.kind != scene_runtime_object_interaction_kind_t::none;
            }

            return summary;
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

    std::string_view to_string(const scene_change_request_kind_t request_kind) noexcept
    {
        switch (request_kind)
        {
            case scene_change_request_kind_t::none: return "none";
            case scene_change_request_kind_t::load: return "load";
            case scene_change_request_kind_t::transition: return "transition";
            case scene_change_request_kind_t::rebuild: return "rebuild";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_change_outcome_t outcome) noexcept
    {
        switch (outcome)
        {
            case scene_change_outcome_t::none: return "none";
            case scene_change_outcome_t::in_progress: return "in_progress";
            case scene_change_outcome_t::succeeded: return "succeeded";
            case scene_change_outcome_t::failed: return "failed";
        }

        return "unknown";
    }

    std::string_view to_string(const scene_runtime_object_interaction_kind_t kind) noexcept
    {
        switch (kind)
        {
            case scene_runtime_object_interaction_kind_t::sign: return "sign";
            case scene_runtime_object_interaction_kind_t::door: return "door";
            case scene_runtime_object_interaction_kind_t::container: return "container";
            case scene_runtime_object_interaction_kind_t::trigger: return "trigger";
            case scene_runtime_object_interaction_kind_t::none:
            default:
                return "none";
        }
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

        const scene_load_options_t resolved_options{ resolve_load_options(options) };
        const assets::scene_asset_record_t* scene_record{ game.assets.scenes().registry().find(scene_id) };
        if (!scene_record)
            return false;

        const std::string resolved_spawn_marker{
            resolved_options.spawn_marker_override.empty()
                ? std::string{ scene_record->scene.player_spawn_marker }
                : std::string{ resolved_options.spawn_marker_override }
        };
        return request_scene_change(game,
                                    *scene_record,
                                    scene_id,
                                    resolved_spawn_marker,
                                    resolved_options,
                                    scene_change_request_kind_t::load);
    }

    bool scene_runtime_t::request_transition(core::game_context_t& game,
                                             const scene_transition_request_t& request,
                                             const scene_load_options_t& options)
    {
        scene_load_options_t transition_options{ resolve_load_options(options) };
        transition_options.spawn_marker_override = request.marker_name;
        const assets::scene_asset_record_t* scene_record{ game.assets.scenes().registry().find(request.scene_id) };
        if (!scene_record || !can_accept_scene_change_request())
            return false;

        const std::string resolved_spawn_marker{
            transition_options.spawn_marker_override.empty()
                ? std::string{ scene_record->scene.player_spawn_marker }
                : std::string{ transition_options.spawn_marker_override }
        };
        return request_scene_change(game,
                                    *scene_record,
                                    request.scene_id,
                                    resolved_spawn_marker,
                                    transition_options,
                                    scene_change_request_kind_t::transition);
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
        adopt_pending_scene(game, std::move(staged_world));
        finalize_active_scene_activation(game);

        _transition_phase = scene_transition_phase_t::finalizing;
        notify_scene_change_complete(game);

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

    bool scene_runtime_t::request_rebuild_current_scene(core::game_context_t& game)
    {
        if (!can_request_rebuild_current_scene())
            return false;

        if (_current_scene_record == nullptr || _current_scene_id.empty() || _current_spawn_marker.empty())
            return false;

        scene_load_options_t rebuild_options{ _active_options };
        rebuild_options.spawn_marker_override = _current_spawn_marker;
        clear_pending_structural_refresh_context();

        LOG_CORE_INFO("Scene rebuild requested for current scene '{}' at spawn '{}'",
                      _current_scene_id,
                      _current_spawn_marker);
        return request_scene_change(game,
                                    *_current_scene_record,
                                    _current_scene_id,
                                    _current_spawn_marker,
                                    rebuild_options,
                                    scene_change_request_kind_t::rebuild);
    }

    bool scene_runtime_t::request_rebuild_current_scene_for_asset(core::game_context_t& game,
                                                                  const assets::asset_iteration_status_t& status)
    {
        if (!can_request_rebuild_current_scene())
            return false;

        if (_current_scene_record == nullptr || _current_scene_id.empty() || _current_spawn_marker.empty())
            return false;

        const auto action{ assets::recommended_runtime_refresh_action(status, true) };
        if (action != assets::asset_runtime_refresh_action_t::rebuild_current_scene)
        {
            LOG_CORE_WARN("Asset-driven scene rebuild request rejected for asset '{}' (kind='{}', action='{}')",
                          status.logical_id,
                          assets::to_string(status.kind),
                          assets::to_string(action));
            return false;
        }

        scene_load_options_t rebuild_options{ _active_options };
        rebuild_options.spawn_marker_override = _current_spawn_marker;
        _pending_structural_refresh_asset_kind = status.kind;
        _pending_structural_refresh_asset_logical_id = status.logical_id;
        _pending_structural_refresh_reason = std::string{ assets::describe_runtime_refresh_action_reason(status, true) };

        LOG_CORE_INFO("Asset-driven scene rebuild requested for current scene '{}' at spawn '{}' by {} asset '{}'",
                      _current_scene_id,
                      _current_spawn_marker,
                      assets::to_string(status.kind),
                      status.logical_id);
        return request_scene_change(game,
                                    *_current_scene_record,
                                    _current_scene_id,
                                    _current_spawn_marker,
                                    rebuild_options,
                                    scene_change_request_kind_t::rebuild);
    }

    bool scene_runtime_t::rebuild_current_scene(core::game_context_t& game)
    {
        if (!request_rebuild_current_scene(game))
            return false;

        while (has_pending_scene())
            (void)update(game);

        return _last_scene_change_succeeded;
    }

    bool scene_runtime_t::can_request_rebuild_current_scene() const noexcept
    {
        return has_scene_loaded() &&
               _current_scene_record != nullptr &&
               !_current_scene_id.empty() &&
               !_current_spawn_marker.empty() &&
               can_accept_scene_change_request();
    }

    void scene_runtime_t::set_default_runtime_bindings(scene_runtime_bindings_t bindings) noexcept
    {
        _default_runtime_bindings = bindings;
    }

    void scene_runtime_t::update_camera(core::game_context_t& game, const float delta_time) noexcept
    {
        if (_active_camera_options.follow_mode != assets::scene_camera_follow_mode_t::player || !_player_controller)
            return;

        if (const world::world_object_t* player{ _player_controller->controlled_object() };
            player && player->transform)
        {
            const chlm::float2 current_center{ game.view.camera_center_world_position(game.world) };
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

            game.view.center_camera_on_world_position(game.world, desired_center);
        }
    }

    void scene_runtime_t::finalize_active_scene_activation(core::game_context_t& game) noexcept
    {
        bind_active_runtime_objects(game);
        apply_active_scene_defaults(game);
    }

    void scene_runtime_t::bind_active_player_controller(core::game_context_t& game) noexcept
    {
        if (_player_controller)
            _player_controller->set_controlled_object(find_scene_player(game.world, _current_scene_record->scene));
    }

    void scene_runtime_t::bind_active_interaction_controller() noexcept
    {
        if (_interaction_controller)
            _interaction_controller->set_actor(_player_controller ? _player_controller->controlled_object() : nullptr);
    }

    void scene_runtime_t::adopt_pending_scene(core::game_context_t& game, world::world_t staged_world) noexcept
    {
        game.world = std::move(staged_world);
        _player_controller = _pending_options.player_controller;
        _interaction_controller = _pending_options.interaction_controller;
        _current_scene_record = _pending_scene_record;
        _current_scene_id = _pending_scene_id;
        _current_spawn_marker = _pending_spawn_marker;
        _active_options = _pending_options;
        _active_options.spawn_marker_override = {};
        _pending_options.spawn_marker_override = {};
        _pending_load_task.reset();
    }

    void scene_runtime_t::bind_active_runtime_objects(core::game_context_t& game) noexcept
    {
        bind_active_player_controller(game);
        bind_active_interaction_controller();
    }

    void scene_runtime_t::apply_active_scene_defaults(core::game_context_t& game) noexcept
    {
        if (_active_options.apply_camera_defaults)
        {
            apply_camera_defaults(game, _current_scene_record->scene, _active_options.camera_override);
            center_camera_on_initial_target(game);
        }

        if (_active_options.apply_scene_music)
            refresh_scene_music(_current_scene_record->scene);
    }

    void scene_runtime_t::notify_scene_change_complete(core::game_context_t& game)
    {
        if (!_active_options.listener)
            return;

        const scene_runtime_context_t current_context{ make_context(game) };
        _active_options.listener->after_scene_change(game, current_context);
    }

    void scene_runtime_t::apply_camera_defaults(core::game_context_t& game,
                                                const assets::scene_asset_t& scene,
                                                const scene_camera_override_t& override) noexcept
    {
        _active_camera_options = resolve_scene_camera_options(
            resolve_scene_camera_options(_camera_options, make_scene_camera_override(scene.camera)),
            override);
        game.view.set_camera_zoom(_active_camera_options.zoom);
    }

    void scene_runtime_t::center_camera_on_initial_target(core::game_context_t& game) noexcept
    {
        if (_active_camera_options.initial_target_policy == assets::scene_camera_initial_target_policy_t::spawn_marker)
        {
            if (const world::world_object_t* marker{ find_spawn_marker(game.world, _current_spawn_marker) };
                marker && marker->transform)
            {
                game.view.center_camera_on_world_position(game.world, marker->transform->position);
                return;
            }
        }

        if (_player_controller)
        {
            if (const world::world_object_t* player{ _player_controller->controlled_object() };
                player && player->transform)
            {
                game.view.center_camera_on_world_position(game.world, player->transform->position);
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
            .pending_request_kind = has_pending_scene() ? _pending_request_kind : scene_change_request_kind_t::none,
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

    scene_runtime_summary_t scene_runtime_t::summarize(core::game_context_t& game) const noexcept
    {
        const scene_runtime_snapshot_t state_snapshot{ snapshot() };
        const bool diagnostics_visible{
            is_transitioning() || _transition_diagnostics_hold_remaining_seconds > 0.f
        };
        scene_runtime_summary_t summary{
            .snapshot = state_snapshot,
            .diagnostics = scene_runtime_summary_t::transition_diagnostics_t{
                .visible = diagnostics_visible,
                .request_kind = diagnostics_visible && has_pending_scene()
                                    ? _pending_request_kind
                                    : _recent_transition_diagnostics.request_kind,
                .outcome = diagnostics_visible && has_pending_scene()
                               ? scene_change_outcome_t::in_progress
                               : _recent_transition_diagnostics.outcome,
                .preserved_active_scene = diagnostics_visible && has_pending_scene()
                                              ? has_scene_loaded()
                                              : _recent_transition_diagnostics.preserved_active_scene,
                .overlay_style = diagnostics_visible && has_pending_scene()
                                     ? _active_transition_overlay_options.style
                                     : _recent_transition_diagnostics.overlay_style,
                .wipe_direction = diagnostics_visible && has_pending_scene()
                                      ? _active_transition_overlay_options.wipe_direction
                                      : _recent_transition_diagnostics.wipe_direction,
                .active_scene_id = diagnostics_visible && has_pending_scene()
                                       ? std::string{ state_snapshot.active_scene_id }
                                       : _recent_transition_diagnostics.active_scene_id,
                .target_scene_id = diagnostics_visible && has_pending_scene()
                                       ? std::string{ state_snapshot.pending_scene_id }
                                       : _recent_transition_diagnostics.pending_scene_id,
                .target_spawn_marker = diagnostics_visible && has_pending_scene()
                                           ? std::string{ state_snapshot.pending_spawn_marker }
                                           : _recent_transition_diagnostics.pending_spawn_marker,
                .structural_refresh_asset_kind = diagnostics_visible && has_pending_scene()
                                                     ? _pending_structural_refresh_asset_kind
                                                     : _recent_transition_diagnostics.structural_refresh_asset_kind,
                .structural_refresh_asset_logical_id = diagnostics_visible && has_pending_scene()
                                                           ? _pending_structural_refresh_asset_logical_id
                                                           : _recent_transition_diagnostics.structural_refresh_asset_logical_id,
                .structural_refresh_reason = diagnostics_visible && has_pending_scene()
                                                 ? _pending_structural_refresh_reason
                                                 : _recent_transition_diagnostics.structural_refresh_reason,
                .transition_progress = diagnostics_visible && has_pending_scene()
                                           ? state_snapshot.transition_progress
                                           : _recent_transition_diagnostics.transition_progress,
                .startup_waiting_for_first_present = _recent_transition_diagnostics.startup_waiting_for_first_present
            },
            .active_camera = _active_camera_options,
            .camera_center_world = game.view.camera_center_world_position(game.world),
            .static_collider_count = static_cast<uint32_t>(game.world.collision_world().static_colliders().size()),
            .point_light_count = static_cast<uint32_t>(game.world.lighting().point_lights.size())
        };

        for (const world::world_object_t& object : game.world.objects())
        {
            summary.world_object_count++;

            if (object.trigger)
                summary.trigger_count++;

            if (object.collision)
                summary.object_collider_count++;

            if (object.visibility_region)
                summary.visibility_region_count++;
        }

        if (_player_controller)
        {
            if (const world::world_object_t* player{ _player_controller->controlled_object() })
            {
                summary.player_object_id = player->id;
                summary.player_object_name = player->name;
            }
        }

        if (!summary.has_player_object() && _current_scene_record)
        {
            if (const world::world_object_t* player{ find_scene_player(game.world, _current_scene_record->scene) })
            {
                summary.player_object_id = player->id;
                summary.player_object_name = player->name;
            }
        }

        if (const world::world_object_t* spawn{ find_spawn_marker(game.world, _current_spawn_marker) })
        {
            summary.spawn_object_id = spawn->id;
            summary.spawn_object_name = spawn->name;
        }

        return summary;
    }

    std::vector<scene_runtime_object_summary_t> scene_runtime_t::collect_runtime_object_summaries(
        core::game_context_t& game) const
    {
        std::vector<scene_runtime_object_summary_t> summaries;
        summaries.reserve(game.world.objects().size());

        for (const world::world_object_t& object : game.world.objects())
            summaries.emplace_back(summarize_world_object(object));

        return summaries;
    }

    std::optional<scene_runtime_object_summary_t> scene_runtime_t::find_runtime_object_summary(
        core::game_context_t& game,
        const world::world_object_id_t object_id) const
    {
        for (const world::world_object_t& object : game.world.objects())
        {
            if (object.id == object_id)
                return summarize_world_object(object);
        }

        return std::nullopt;
    }

    scene_runtime_systems_summary_t scene_runtime_t::summarize_runtime_systems(core::game_context_t& game) const
    {
        scene_runtime_systems_summary_t summary;

        const world::world_lighting_state_t& lighting{ game.world.lighting() };
        summary.lighting.ambient_color = lighting.ambient_color;
        summary.lighting.point_light_count = static_cast<uint32_t>(lighting.point_lights.size());
        summary.lighting.point_lights.reserve(lighting.point_lights.size());
        for (const world::world_lighting_state_t::point_light_t& light : lighting.point_lights)
        {
            summary.lighting.point_lights.push_back(scene_runtime_light_summary_t{
                .position_world = light.position_world,
                .radius_world = light.radius_world,
                .color = light.color,
                .intensity = light.intensity
            });
        }

        const world::collision_debug_view_t& collision_debug{ game.world.collision_debug_view() };
        summary.collision = scene_runtime_collision_system_summary_t{
            .static_collider_count = static_cast<uint32_t>(game.world.collision_world().static_colliders().size()),
            .show_map_collision = collision_debug.show_map_collision,
            .show_object_colliders = collision_debug.show_object_colliders,
            .show_trigger_volumes = collision_debug.show_trigger_volumes,
            .map_collision_color = collision_debug.map_collision_color,
            .map_outline_thickness = collision_debug.map_outline_thickness,
            .trigger_volume_color = collision_debug.trigger_volume_color,
            .trigger_outline_thickness = collision_debug.trigger_outline_thickness,
            .trigger_filled = collision_debug.trigger_filled
        };

        const world::layering_debug_view_t& layering_debug{ game.world.layering_debug_view() };
        const world::layering_debug_snapshot_t& layering_snapshot{ game.world.layering_debug_snapshot() };
        summary.layering.show_visibility_regions = layering_debug.show_visibility_regions;
        summary.layering.visibility_region_color = layering_debug.visibility_region_color;
        summary.layering.frame_index = layering_snapshot.frame_index;
        summary.layering.has_visibility_anchor = layering_snapshot.has_visibility_anchor;
        summary.layering.visibility_anchor_world = layering_snapshot.visibility_anchor_world;
        summary.layering.visibility_region_count = layering_snapshot.visibility_region_count;
        summary.layering.rendered_tilemap_count = layering_snapshot.rendered_tilemap_count;
        summary.layering.layer_count = layering_snapshot.layer_count;
        summary.layering.visible_layer_count = layering_snapshot.visible_layer_count;
        summary.layering.hidden_layer_count = layering_snapshot.hidden_layer_count;
        summary.layering.visibility_bound_layer_count = layering_snapshot.visibility_bound_layer_count;
        summary.layering.conditional_front_layer_count = layering_snapshot.conditional_front_layer_count;
        summary.layering.always_front_layer_count = layering_snapshot.always_front_layer_count;
        summary.layering.active_visibility_tags.assign(
            layering_snapshot.active_visibility_tags.begin(),
            layering_snapshot.active_visibility_tags.end());

        if (_player_controller)
        {
            summary.player_controller.bound = true;
            summary.player_controller.facing_direction = std::string{ to_string(_player_controller->facing_direction()) };
            summary.player_controller.move_speed = _player_controller->move_speed();
            summary.player_controller.move_intent = _player_controller->move_intent();
            const world::player_move_result_t& last_move{ _player_controller->last_move_result() };
            summary.player_controller.last_requested_delta = last_move.requested_delta;
            summary.player_controller.last_actual_delta = last_move.actual_delta;
            summary.player_controller.last_blocked_x = last_move.blocked_x;
            summary.player_controller.last_blocked_y = last_move.blocked_y;
            summary.player_controller.last_started_overlapping = last_move.started_overlapping;
            const collision::collision_aabb_t collision_bounds{ _player_controller->current_collision_bounds() };
            summary.player_controller.collision_bounds_min = collision_bounds.min;
            summary.player_controller.collision_bounds_max = collision_bounds.max;
            if (const world::world_object_t* object{ _player_controller->controlled_object() })
            {
                summary.player_controller.controlled_object_id = object->id;
                summary.player_controller.controlled_object_name = object->name;
            }
        }

        if (_interaction_controller)
        {
            summary.interaction_controller.bound = true;
            summary.interaction_controller.interaction_radius = _interaction_controller->interaction_radius();
            if (const world::world_object_t* actor{ _interaction_controller->actor() })
            {
                summary.interaction_controller.actor_object_id = actor->id;
                summary.interaction_controller.actor_object_name = actor->name;
            }

            if (const world::world_object_t* candidate{ _interaction_controller->find_candidate(game.world) })
            {
                summary.interaction_controller.has_candidate = true;
                summary.interaction_controller.candidate_object_id = candidate->id;
                summary.interaction_controller.candidate_object_name = candidate->name;
                summary.interaction_controller.candidate_distance = _interaction_controller->candidate_distance(game.world);
            }
        }

        return summary;
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
            game.view.clear_composite_overlay();
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
            game.view.clear_composite_overlay();
            const chlm::uint2 render_target_size{ game.view.render_target_pixel_size() };
            const float viewport_width{ static_cast<float>(std::max(1u, render_target_size.x)) };
            const float viewport_height{ static_cast<float>(std::max(1u, render_target_size.y)) };
            const float coverage{ std::clamp(_transition_overlay_opacity, 0.f, 1.f) };
            switch (_active_transition_overlay_options.wipe_direction)
            {
                case scene_transition_wipe_direction_t::left_to_right:
                {
                    const float covered_width{ coverage * viewport_width };
                    if (covered_width > 0.001f && viewport_height > 0.001f)
                        game.view.draw_composite_solid_quad(0.f, 0.f, covered_width, viewport_height, overlay_color);
                    break;
                }
                case scene_transition_wipe_direction_t::right_to_left:
                {
                    const float covered_width{ coverage * viewport_width };
                    if (covered_width > 0.001f && viewport_height > 0.001f)
                        game.view.draw_composite_solid_quad(viewport_width - covered_width,
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
                        game.view.draw_composite_solid_quad(0.f, 0.f, viewport_width, covered_height, overlay_color);
                    break;
                }
                case scene_transition_wipe_direction_t::bottom_to_top:
                {
                    const float covered_height{ coverage * viewport_height };
                    if (viewport_width > 0.001f && covered_height > 0.001f)
                        game.view.draw_composite_solid_quad(0.f,
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
            game.view.set_composite_overlay_color(overlay_color);
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

    void scene_runtime_t::capture_recent_transition_diagnostics(const scene_change_outcome_t outcome) noexcept
    {
        const scene_runtime_snapshot_t state_snapshot{ snapshot() };
        _recent_transition_diagnostics.request_kind = state_snapshot.pending_request_kind;
        _recent_transition_diagnostics.outcome = outcome;
        _recent_transition_diagnostics.runtime_state = state_snapshot.runtime_state;
        _recent_transition_diagnostics.transition_phase = state_snapshot.transition_phase;
        _recent_transition_diagnostics.overlay_style = _active_transition_overlay_options.style;
        _recent_transition_diagnostics.wipe_direction = _active_transition_overlay_options.wipe_direction;
        _recent_transition_diagnostics.preserved_active_scene =
            outcome == scene_change_outcome_t::failed && has_scene_loaded();
        _recent_transition_diagnostics.active_scene_id = std::string{ state_snapshot.active_scene_id };
        _recent_transition_diagnostics.pending_scene_id = std::string{ state_snapshot.pending_scene_id };
        _recent_transition_diagnostics.pending_spawn_marker = std::string{ state_snapshot.pending_spawn_marker };
        _recent_transition_diagnostics.structural_refresh_asset_kind = _pending_structural_refresh_asset_kind;
        _recent_transition_diagnostics.structural_refresh_asset_logical_id = _pending_structural_refresh_asset_logical_id;
        _recent_transition_diagnostics.structural_refresh_reason = _pending_structural_refresh_reason;
        _recent_transition_diagnostics.transition_progress = state_snapshot.transition_progress;
        _recent_transition_diagnostics.startup_waiting_for_first_present = _startup_overlay_waiting_for_first_present;
    }

    void scene_runtime_t::clear_pending_structural_refresh_context() noexcept
    {
        _pending_structural_refresh_asset_kind = assets::asset_kind_t::texture;
        _pending_structural_refresh_asset_logical_id.clear();
        _pending_structural_refresh_reason.clear();
    }

    void scene_runtime_t::render_transition_diagnostics(core::game_context_t& game) noexcept
    {
        if (!is_transitioning() && _transition_diagnostics_hold_remaining_seconds <= 0.f)
            return;

        constexpr float panel_y{ 12.f };
        constexpr float panel_width{ 340.f };
        constexpr float panel_height{ 114.f };
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
                                  "request: %s  result: %s",
                                  to_string(_recent_transition_diagnostics.request_kind).data(),
                                  to_string(_recent_transition_diagnostics.outcome).data());
        debug::text_colored_sized(text_x,
                                  panel_y + 8.f + (line_step * 3.f),
                                  k_transition_diagnostics_font_size,
                                  0xFFD7CDBEu,
                                  "phase: %s  progress: %.0f%%",
                                  to_string(_recent_transition_diagnostics.transition_phase).data(),
                                  std::round(std::clamp(_recent_transition_diagnostics.transition_progress, 0.f, 1.f) * 100.f));

        if (_recent_transition_diagnostics.overlay_style == scene_transition_overlay_style_t::wipe)
        {
            debug::text_colored_sized(text_x,
                                      panel_y + 8.f + (line_step * 4.f),
                                      k_transition_diagnostics_font_size,
                                      0xFFD7CDBEu,
                                      "overlay: %s %s  preserved: %s",
                                      to_string(_recent_transition_diagnostics.overlay_style).data(),
                                      to_string(_recent_transition_diagnostics.wipe_direction).data(),
                                      _recent_transition_diagnostics.preserved_active_scene ? "yes" : "no");
        }
        else
        {
            debug::text_colored_sized(text_x,
                                      panel_y + 8.f + (line_step * 4.f),
                                      k_transition_diagnostics_font_size,
                                      0xFFD7CDBEu,
                                      "overlay: %s%s  preserved: %s",
                                      to_string(_recent_transition_diagnostics.overlay_style).data(),
                                      _recent_transition_diagnostics.startup_waiting_for_first_present ? " (boot wait)" : "",
                                      _recent_transition_diagnostics.preserved_active_scene ? "yes" : "no");
        }

        if (!_recent_transition_diagnostics.structural_refresh_asset_logical_id.empty())
        {
            debug::text_colored_sized(text_x,
                                      panel_y + 8.f + (line_step * 5.f),
                                      k_transition_diagnostics_font_size,
                                      0xFFD7CDBEu,
                                      "refresh: %s %s",
                                      assets::to_string(_recent_transition_diagnostics.structural_refresh_asset_kind).data(),
                                      _recent_transition_diagnostics.structural_refresh_asset_logical_id.c_str());
        }
    }

    scene_load_options_t scene_runtime_t::resolve_load_options(const scene_load_options_t& options) const noexcept
    {
        scene_load_options_t resolved{ options };
        if (resolved.player_controller == nullptr)
            resolved.player_controller = _default_runtime_bindings.player_controller;
        if (resolved.interaction_controller == nullptr)
            resolved.interaction_controller = _default_runtime_bindings.interaction_controller;
        if (resolved.validate_loaded_scene == nullptr)
            resolved.validate_loaded_scene = _default_runtime_bindings.validate_loaded_scene;
        if (resolved.listener == nullptr)
            resolved.listener = _default_runtime_bindings.listener;
        return resolved;
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

    bool scene_runtime_t::request_scene_change(core::game_context_t& game,
                                               const assets::scene_asset_record_t& scene_record,
                                               const std::string_view scene_id,
                                               const std::string_view spawn_marker,
                                               const scene_load_options_t& options,
                                               const scene_change_request_kind_t request_kind)
    {
        const bool had_scene_loaded{ has_scene_loaded() };
        const scene_runtime_context_t previous_context{ make_context(game) };

        _pending_request_kind = request_kind;
        _pending_options = options;
        _pending_options.spawn_marker_override = {};
        _active_transition_overlay_options = resolve_transition_overlay_options(_transition_overlay_options,
                                                                                options.transition_overlay);
        LOG_CORE_INFO("Scene {} requested: current='{}', target='{}', spawn='{}', overlay='{}'",
                      to_string(request_kind),
                      _current_scene_id.empty() ? "<none>" : _current_scene_id,
                      scene_id,
                      spawn_marker,
                      to_string(_active_transition_overlay_options.style));
        _pending_load_task.emplace(scene_id, spawn_marker);
        _last_scene_change_succeeded = false;
        begin_scene_change(scene_record, scene_id, spawn_marker);

        if (options.listener)
            options.listener->before_scene_change(game,
                                                 had_scene_loaded ? &previous_context : nullptr,
                                                 scene_id,
                                                 spawn_marker);

        return true;
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

        if (!_active_transition_overlay_options.enabled)
            return true;

        return _transition_overlay_stage == transition_overlay_stage_t::hidden;
    }

    void scene_runtime_t::fail_scene_change() noexcept
    {
        capture_recent_transition_diagnostics(scene_change_outcome_t::failed);
        LOG_CORE_WARN("Scene {} failed: current='{}', pending='{}', phase='{}'",
                      to_string(_pending_request_kind),
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
        _pending_request_kind = scene_change_request_kind_t::none;
        _last_scene_change_succeeded = false;
        if (!_active_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
            _startup_overlay_waiting_for_first_present = false;
        }
        clear_pending_structural_refresh_context();
        _transition_diagnostics_hold_remaining_seconds = k_transition_diagnostics_linger_seconds;
    }

    void scene_runtime_t::complete_scene_change() noexcept
    {
        capture_recent_transition_diagnostics(scene_change_outcome_t::succeeded);
        LOG_CORE_INFO("Scene {} complete: current='{}', spawn='{}'",
                      to_string(_pending_request_kind),
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
        _pending_request_kind = scene_change_request_kind_t::none;
        _last_scene_change_succeeded = true;
        if (!_active_transition_overlay_options.enabled)
        {
            _transition_overlay_stage = transition_overlay_stage_t::hidden;
            _transition_overlay_opacity = 0.f;
            _transition_overlay_hold_elapsed_seconds = 0.f;
            _startup_overlay_waiting_for_first_present = false;
        }
        clear_pending_structural_refresh_context();
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
