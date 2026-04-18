//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Scene/SceneAsset.h"
#include "Audio/Voice/VoiceHandle.h"
#include "World/SceneLoader.h"
#include "World/WorldObject.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace carrot::assets {
    class asset_manager_t;
}

namespace carrot::core {
    struct game_context_t;
    class game_view_t;
}

namespace carrot::world {
    class world_t;
    class player_controller_t;
    class interaction_controller_t;
}

namespace carrot::scene {
    struct scene_runtime_context_t;
    class scene_runtime_listener_t;

    enum class scene_runtime_state_t : uint8_t
    {
        idle = 0,
        loading,
        active,
        transitioning
    };

    enum class scene_transition_phase_t : uint8_t
    {
        none = 0,
        preparing,
        loading,
        activating,
        finalizing
    };

    enum class scene_change_request_kind_t : uint8_t
    {
        none = 0,
        load,
        transition,
        rebuild
    };

    enum class scene_change_outcome_t : uint8_t
    {
        none = 0,
        in_progress,
        succeeded,
        failed
    };

    [[nodiscard]] std::string_view to_string(scene_runtime_state_t state) noexcept;
    [[nodiscard]] std::string_view to_string(scene_transition_phase_t phase) noexcept;
    [[nodiscard]] std::string_view to_string(scene_change_request_kind_t request_kind) noexcept;
    [[nodiscard]] std::string_view to_string(scene_change_outcome_t outcome) noexcept;

    struct scene_transition_presentation_t
    {
        bool visible{ false };
        bool show_loading_text{ false };
        float overlay_opacity{ 0.f };
        float progress{ 0.f };
        uint32_t overlay_color_abgr{ 0x00000000u };
        std::string_view phase_label;
    };

    enum class scene_transition_overlay_style_t : uint8_t
    {
        inherit = 0,
        none,
        fade,
        loading_screen,
        wipe
    };

    [[nodiscard]] std::string_view to_string(scene_transition_overlay_style_t style) noexcept;

    enum class scene_transition_wipe_direction_t : uint8_t
    {
        left_to_right = 0,
        right_to_left,
        top_to_bottom,
        bottom_to_top
    };

    [[nodiscard]] std::string_view to_string(scene_transition_wipe_direction_t direction) noexcept;

    enum class scene_camera_projection_mode_t : uint8_t
    {
        orthographic = 0,
        perspective
    };

    enum class scene_camera_bounds_mode_t : uint8_t
    {
        none = 0,
        scene_extents
    };

    [[nodiscard]] std::string_view to_string(scene_camera_projection_mode_t mode) noexcept;
    [[nodiscard]] std::string_view to_string(scene_camera_bounds_mode_t mode) noexcept;

    struct scene_camera_options_t
    {
        scene_camera_projection_mode_t projection_mode{ scene_camera_projection_mode_t::orthographic };
        scene_camera_bounds_mode_t bounds_mode{ scene_camera_bounds_mode_t::none };
        float zoom{ 4.f };
        assets::scene_camera_follow_mode_t follow_mode{ assets::scene_camera_follow_mode_t::player };
        assets::scene_camera_initial_target_policy_t initial_target_policy{
            assets::scene_camera_initial_target_policy_t::player
        };
        chlm::float2 dead_zone_size_world{ 2.0f, 1.5f };
        float follow_smoothing{ 10.0f };
    };

    struct scene_camera_override_t
    {
        std::optional<scene_camera_projection_mode_t> projection_mode;
        std::optional<scene_camera_bounds_mode_t> bounds_mode;
        std::optional<float> zoom;
        std::optional<assets::scene_camera_follow_mode_t> follow_mode;
        std::optional<assets::scene_camera_initial_target_policy_t> initial_target_policy;
        std::optional<chlm::float2> dead_zone_size_world;
        std::optional<float> follow_smoothing;
    };

    struct scene_transition_overlay_options_t
    {
        bool enabled{ true };
        scene_transition_overlay_style_t style{ scene_transition_overlay_style_t::fade };
        scene_transition_wipe_direction_t wipe_direction{ scene_transition_wipe_direction_t::left_to_right };
        uint32_t overlay_color_abgr{ 0xFF000000u };
        float fade_out_to_black_seconds{ 0.5f };
        float minimum_opaque_hold_seconds{ 0.0f };
        float fade_in_from_black_seconds{ 0.5f };
        bool show_loading_text{ false };
        uint32_t loading_text_color_abgr{ 0xFFF5F1E8u };
        uint32_t loading_subtext_color_abgr{ 0xFFD7CDBEu };
        bool show_progress_text{ false };
        std::string loading_title_text;
        std::string loading_subtitle_text;
    };

    struct scene_transition_overlay_override_t
    {
        scene_transition_overlay_style_t style{ scene_transition_overlay_style_t::inherit };
        std::optional<scene_transition_wipe_direction_t> wipe_direction;
        std::optional<uint32_t> overlay_color_abgr;
        std::optional<float> fade_out_to_black_seconds;
        std::optional<float> minimum_opaque_hold_seconds;
        std::optional<float> fade_in_from_black_seconds;
        std::optional<bool> show_loading_text;
        std::optional<uint32_t> loading_text_color_abgr;
        std::optional<uint32_t> loading_subtext_color_abgr;
        std::optional<bool> show_progress_text;
        std::optional<std::string> loading_title_text;
        std::optional<std::string> loading_subtitle_text;
    };

    struct scene_transition_request_t
    {
        std::string scene_id;
        std::string marker_name;
    };

    using scene_validation_callback_t = bool (*)(const assets::asset_manager_t& assets,
                                                 const world::world_t& world,
                                                 std::string_view scene_id);

    class scene_runtime_listener_t
    {
    public:
        virtual ~scene_runtime_listener_t() = default;

        /**
         * @brief Called after a scene change has been requested, but before the active world is replaced.
         *
         * current_context is null when no scene is currently active.
         *
         * This hook exists for game-side state capture and transition preparation.
         * It is not the place to mutate the newly staged world or assume the target
         * scene has already become active.
         */
        virtual void before_scene_change(core::game_context_t& game,
                                         const scene_runtime_context_t* current_context,
                                         std::string_view next_scene_id,
                                         std::string_view next_spawn_marker)
        {
            (void)game;
            (void)current_context;
            (void)next_scene_id;
            (void)next_spawn_marker;
        }

        /**
         * @brief Called after the staged world has been adopted and post-activation engine state has been applied.
         *
         * At this point the new scene is active, controller bindings have been
         * rebound, camera defaults/overrides have been applied, scene music has
         * been refreshed when enabled, and current_context reflects the
         * post-activation runtime truth.
         */
        virtual void after_scene_change(core::game_context_t& game,
                                        const scene_runtime_context_t& current_context)
        {
            (void)game;
            (void)current_context;
        }
    };

    struct scene_load_options_t
    {
        std::string_view spawn_marker_override;
        world::player_controller_t* player_controller{ nullptr };
        world::interaction_controller_t* interaction_controller{ nullptr };
        scene_validation_callback_t validate_loaded_scene{ nullptr };
        scene_runtime_listener_t* listener{ nullptr };
        bool apply_camera_defaults{ true };
        bool apply_scene_music{ true };
        scene_camera_override_t camera_override;
        scene_transition_overlay_override_t transition_overlay;
    };

    struct scene_runtime_context_t
    {
        world::world_t& world;
        assets::asset_manager_t& assets;
        core::game_view_t& view;
        const assets::scene_asset_record_t* scene_record{ nullptr };
        std::string_view scene_id;
        std::string_view spawn_marker;

        [[nodiscard]] world::world_object_t* find_object_by_id(world::world_object_id_t object_id) const noexcept;
        [[nodiscard]] world::world_object_t* player() const noexcept;
        [[nodiscard]] const world::world_object_t* spawn_object() const noexcept;
    };

    struct scene_runtime_snapshot_t
    {
        scene_runtime_state_t runtime_state{ scene_runtime_state_t::idle };
        scene_transition_phase_t transition_phase{ scene_transition_phase_t::none };
        scene_change_request_kind_t pending_request_kind{ scene_change_request_kind_t::none };
        const assets::scene_asset_record_t* active_scene_record{ nullptr };
        const assets::scene_asset_record_t* pending_scene_record{ nullptr };
        std::string_view active_scene_id;
        std::string_view active_spawn_marker;
        std::string_view pending_scene_id;
        std::string_view pending_spawn_marker;
        size_t transition_completed_steps{ 0u };
        size_t transition_total_steps{ 0u };
        float transition_progress{ 0.f };

        [[nodiscard]] bool has_active_scene() const noexcept { return !active_scene_id.empty(); }
        [[nodiscard]] bool has_pending_scene() const noexcept { return !pending_scene_id.empty(); }
        [[nodiscard]] bool is_transitioning() const noexcept
        {
            return runtime_state == scene_runtime_state_t::loading ||
                   runtime_state == scene_runtime_state_t::transitioning;
        }
    };

    struct scene_runtime_summary_t
    {
        struct transition_diagnostics_t
        {
            bool visible{ false };
            scene_change_request_kind_t request_kind{ scene_change_request_kind_t::none };
            scene_change_outcome_t outcome{ scene_change_outcome_t::none };
            bool preserved_active_scene{ false };
            scene_transition_overlay_style_t overlay_style{ scene_transition_overlay_style_t::fade };
            scene_transition_wipe_direction_t wipe_direction{ scene_transition_wipe_direction_t::left_to_right };
            std::string active_scene_id;
            std::string target_scene_id;
            std::string target_spawn_marker;
            float transition_progress{ 0.f };
            bool startup_waiting_for_first_present{ false };
        };

        scene_runtime_snapshot_t snapshot;
        transition_diagnostics_t diagnostics;
        scene_camera_options_t active_camera;
        chlm::float2 camera_center_world{ 0.f, 0.f };
        world::world_object_id_t player_object_id{ 0 };
        std::string_view player_object_name;
        world::world_object_id_t spawn_object_id{ 0 };
        std::string_view spawn_object_name;
        uint32_t world_object_count{ 0u };
        uint32_t trigger_count{ 0u };
        uint32_t object_collider_count{ 0u };
        uint32_t static_collider_count{ 0u };
        uint32_t point_light_count{ 0u };
        uint32_t visibility_region_count{ 0u };

        [[nodiscard]] bool has_player_object() const noexcept { return player_object_id != 0; }
        [[nodiscard]] bool has_spawn_object() const noexcept { return spawn_object_id != 0; }
    };

    enum class scene_runtime_object_interaction_kind_t : uint8_t
    {
        none = 0,
        sign,
        door,
        container,
        trigger
    };

    [[nodiscard]] std::string_view to_string(scene_runtime_object_interaction_kind_t kind) noexcept;

    struct scene_runtime_object_source_summary_t
    {
        std::string tilemap_logical_id;
        std::string layer_name;
        uint32_t object_id{ 0u };
        std::string object_name;
    };

    struct scene_runtime_object_transform_summary_t
    {
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 scale{ 1.f, 1.f };
        float rotation{ 0.f };
    };

    struct scene_runtime_object_collision_summary_t
    {
        chlm::float2 half_extents{ 0.f, 0.f };
        chlm::float2 offset{ 0.f, 0.f };
        bool has_debug_display{ false };
        bool debug_filled{ false };
        float debug_outline_thickness{ 0.f };
        uint32_t debug_color{ 0u };
    };

    struct scene_runtime_object_trigger_summary_t
    {
        std::string trigger_id;
        std::string trigger_kind;
    };

    struct scene_runtime_object_sprite_summary_t
    {
        std::string texture_id;
        std::string frame_name;
        bool use_size_override{ false };
        chlm::float2 size_override_world{ 0.f, 0.f };
        bool use_custom_pivot{ false };
        chlm::float2 pivot{ 0.f, 0.f };
        bool flip_x{ false };
        bool flip_y{ false };
        renderer::render_layer_t layer{ renderer::render_layer_t::actors };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct scene_runtime_object_sprite_animator_summary_t
    {
        std::string current_animation_name;
        std::string current_frame_name;
        bool is_playing{ false };
        bool is_paused{ false };
        bool is_finished{ false };
    };

    struct scene_runtime_object_tile_object_summary_t
    {
        std::string tilemap_logical_id;
        uint32_t gid{ 0u };
        chlm::float2 size_source_px{ 0.f, 0.f };
        renderer::render_layer_t layer{ renderer::render_layer_t::actors };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct scene_runtime_object_tilemap_summary_t
    {
        std::string tilemap_logical_id;
        bool include_object_layers{ false };
        renderer::render_layer_t layer{ renderer::render_layer_t::world_back };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct scene_runtime_object_visibility_region_summary_t
    {
        chlm::float2 size_world{ 0.f, 0.f };
        std::string tag;
    };

    struct scene_runtime_object_interaction_summary_t
    {
        scene_runtime_object_interaction_kind_t kind{ scene_runtime_object_interaction_kind_t::none };
        std::string message_id;
        std::string target_scene;
        std::string target_map;
        std::string target_marker;
        std::string loot_table;
        std::string trigger_id;
        std::string trigger_kind;
    };

    struct scene_runtime_object_summary_t
    {
        world::world_object_id_t id{ 0 };
        std::string name;
        std::string type;
        uint32_t property_count{ 0u };
        bool has_source{ false };
        bool has_transform{ false };
        bool has_collision{ false };
        bool has_trigger{ false };
        bool has_sprite{ false };
        bool has_sprite_animator{ false };
        bool has_tile_object{ false };
        bool has_tilemap{ false };
        bool has_visibility_region{ false };
        bool has_interaction{ false };
        scene_runtime_object_source_summary_t source;
        scene_runtime_object_transform_summary_t transform;
        scene_runtime_object_collision_summary_t collision;
        scene_runtime_object_trigger_summary_t trigger;
        scene_runtime_object_sprite_summary_t sprite;
        scene_runtime_object_sprite_animator_summary_t sprite_animator;
        scene_runtime_object_tile_object_summary_t tile_object;
        scene_runtime_object_tilemap_summary_t tilemap;
        scene_runtime_object_visibility_region_summary_t visibility_region;
        scene_runtime_object_interaction_summary_t interaction;
    };

    struct scene_runtime_light_summary_t
    {
        chlm::float2 position_world{ 0.f, 0.f };
        float radius_world{ 0.f };
        chlm::float4 color{ 1.f, 1.f, 1.f, 1.f };
        float intensity{ 0.f };
    };

    struct scene_runtime_lighting_summary_t
    {
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };
        uint32_t point_light_count{ 0u };
        std::vector<scene_runtime_light_summary_t> point_lights;
    };

    struct scene_runtime_collision_system_summary_t
    {
        uint32_t static_collider_count{ 0u };
        bool show_map_collision{ false };
        bool show_object_colliders{ false };
        bool show_trigger_volumes{ false };
        uint32_t map_collision_color{ 0u };
        float map_outline_thickness{ 0.f };
        uint32_t trigger_volume_color{ 0u };
        float trigger_outline_thickness{ 0.f };
        bool trigger_filled{ false };
    };

    struct scene_runtime_layering_system_summary_t
    {
        bool show_visibility_regions{ false };
        uint32_t visibility_region_color{ 0u };
        uint64_t frame_index{ 0u };
        bool has_visibility_anchor{ false };
        chlm::float2 visibility_anchor_world{ 0.f, 0.f };
        uint32_t visibility_region_count{ 0u };
        uint32_t rendered_tilemap_count{ 0u };
        uint32_t layer_count{ 0u };
        uint32_t visible_layer_count{ 0u };
        uint32_t hidden_layer_count{ 0u };
        uint32_t visibility_bound_layer_count{ 0u };
        uint32_t conditional_front_layer_count{ 0u };
        uint32_t always_front_layer_count{ 0u };
        std::vector<std::string> active_visibility_tags;
    };

    struct scene_runtime_player_controller_summary_t
    {
        bool bound{ false };
        world::world_object_id_t controlled_object_id{ 0u };
        std::string controlled_object_name;
        std::string facing_direction;
        float move_speed{ 0.f };
        chlm::float2 move_intent{ 0.f, 0.f };
        chlm::float2 last_requested_delta{ 0.f, 0.f };
        chlm::float2 last_actual_delta{ 0.f, 0.f };
        bool last_blocked_x{ false };
        bool last_blocked_y{ false };
        bool last_started_overlapping{ false };
        chlm::float2 collision_bounds_min{ 0.f, 0.f };
        chlm::float2 collision_bounds_max{ 0.f, 0.f };

        [[nodiscard]] bool has_controlled_object() const noexcept { return controlled_object_id != 0; }
    };

    struct scene_runtime_interaction_controller_summary_t
    {
        bool bound{ false };
        world::world_object_id_t actor_object_id{ 0u };
        std::string actor_object_name;
        float interaction_radius{ 0.f };
        bool has_candidate{ false };
        world::world_object_id_t candidate_object_id{ 0u };
        std::string candidate_object_name;
        std::optional<float> candidate_distance;

        [[nodiscard]] bool has_actor() const noexcept { return actor_object_id != 0; }
    };

    struct scene_runtime_systems_summary_t
    {
        scene_runtime_lighting_summary_t lighting;
        scene_runtime_collision_system_summary_t collision;
        scene_runtime_layering_system_summary_t layering;
        scene_runtime_player_controller_summary_t player_controller;
        scene_runtime_interaction_controller_summary_t interaction_controller;
    };

    [[nodiscard]] scene_transition_presentation_t make_transition_presentation(
        const scene_runtime_snapshot_t& snapshot,
        uint32_t overlay_color_abgr = 0xFF101820u) noexcept;
    [[nodiscard]] scene_camera_options_t make_default_scene_camera_options() noexcept;
    [[nodiscard]] scene_camera_options_t resolve_scene_camera_options(
        const scene_camera_options_t& defaults,
        const scene_camera_override_t& override) noexcept;
    [[nodiscard]] scene_transition_overlay_options_t resolve_transition_overlay_options(
        const scene_transition_overlay_options_t& defaults,
        const scene_transition_overlay_override_t& override) noexcept;
    [[nodiscard]] scene_transition_overlay_options_t make_default_transition_overlay_options() noexcept;

    class scene_runtime_t
    {
    public:
        [[nodiscard]] bool request_load(core::game_context_t& game,
                                        std::string_view scene_id,
                                        const scene_load_options_t& options = {});
        [[nodiscard]] bool request_transition(core::game_context_t& game,
                                              const scene_transition_request_t& request,
                                              const scene_load_options_t& options = {});
        [[nodiscard]] bool update(core::game_context_t& game);
        [[nodiscard]] bool load(core::game_context_t& game,
                                std::string_view scene_id,
                                const scene_load_options_t& options = {});
        [[nodiscard]] bool transition(core::game_context_t& game,
                                      const scene_transition_request_t& request,
                                      const scene_load_options_t& options = {});
        [[nodiscard]] bool request_rebuild_current_scene(core::game_context_t& game);
        [[nodiscard]] bool rebuild_current_scene(core::game_context_t& game);
        [[nodiscard]] bool can_request_rebuild_current_scene() const noexcept;
        void update_camera(core::game_context_t& game, float delta_time) noexcept;

        [[nodiscard]] std::string_view current_scene_id() const noexcept { return _current_scene_id; }
        [[nodiscard]] std::string_view current_spawn_marker() const noexcept { return _current_spawn_marker; }
        [[nodiscard]] std::string_view pending_scene_id() const noexcept { return _pending_scene_id; }
        [[nodiscard]] std::string_view pending_spawn_marker() const noexcept { return _pending_spawn_marker; }
        [[nodiscard]] bool has_scene_loaded() const noexcept { return !_current_scene_id.empty(); }
        [[nodiscard]] bool has_pending_scene() const noexcept { return !_pending_scene_id.empty(); }
        [[nodiscard]] bool is_transitioning() const noexcept
        {
            return _runtime_state == scene_runtime_state_t::loading ||
                   _runtime_state == scene_runtime_state_t::transitioning;
        }
        [[nodiscard]] bool last_scene_change_succeeded() const noexcept { return _last_scene_change_succeeded; }
        [[nodiscard]] scene_runtime_state_t runtime_state() const noexcept { return _runtime_state; }
        [[nodiscard]] scene_transition_phase_t transition_phase() const noexcept { return _transition_phase; }
        [[nodiscard]] scene_runtime_context_t make_context(core::game_context_t& game) const noexcept;
        [[nodiscard]] scene_runtime_snapshot_t snapshot() const noexcept;
        [[nodiscard]] scene_runtime_summary_t summarize(core::game_context_t& game) const noexcept;
        [[nodiscard]] std::vector<scene_runtime_object_summary_t> collect_runtime_object_summaries(
            core::game_context_t& game) const;
        [[nodiscard]] std::optional<scene_runtime_object_summary_t> find_runtime_object_summary(
            core::game_context_t& game,
            world::world_object_id_t object_id) const;
        [[nodiscard]] scene_runtime_systems_summary_t summarize_runtime_systems(core::game_context_t& game) const;
        [[nodiscard]] const scene_transition_overlay_options_t& engine_transition_overlay_options() const noexcept
        {
            return _engine_transition_overlay_options;
        }
        [[nodiscard]] const scene_transition_overlay_options_t& default_transition_overlay_options() const noexcept
        {
            return _transition_overlay_options;
        }
        [[nodiscard]] const scene_camera_options_t& engine_camera_options() const noexcept { return _engine_camera_options; }
        [[nodiscard]] const scene_camera_options_t& default_camera_options() const noexcept { return _camera_options; }
        [[nodiscard]] const scene_camera_options_t& active_camera_options() const noexcept { return _active_camera_options; }
        void set_default_camera_options(scene_camera_options_t options) noexcept;
        void set_default_camera_override(scene_camera_override_t override) noexcept;
        void set_default_transition_overlay_options(scene_transition_overlay_options_t options) noexcept;
        void set_default_transition_overlay_override(scene_transition_overlay_override_t override) noexcept;
        void advance_transition_overlay(float delta_time) noexcept;
        void render_transition_overlay(core::game_context_t& game) noexcept;

    private:
        /**
         * scene_runtime_t owns the active/pending scene contract and the adoption of
         * staged worlds into the live runtime. scene_load_task_t only prepares an
         * isolated world; scene_runtime_t decides when that staged world becomes the
         * authoritative runtime world and which runtime systems must be rebound.
         */
        enum class transition_overlay_stage_t : uint8_t
        {
            hidden = 0,
            fading_out_to_black,
            holding_opaque,
            fading_in_from_black
        };

        struct recent_transition_diagnostics_t
        {
            scene_change_request_kind_t request_kind{ scene_change_request_kind_t::none };
            scene_change_outcome_t outcome{ scene_change_outcome_t::none };
            scene_runtime_state_t runtime_state{ scene_runtime_state_t::idle };
            scene_transition_phase_t transition_phase{ scene_transition_phase_t::none };
            scene_transition_overlay_style_t overlay_style{ scene_transition_overlay_style_t::fade };
            scene_transition_wipe_direction_t wipe_direction{ scene_transition_wipe_direction_t::left_to_right };
            bool preserved_active_scene{ false };
            std::string active_scene_id;
            std::string pending_scene_id;
            std::string pending_spawn_marker;
            float transition_progress{ 0.f };
            bool startup_waiting_for_first_present{ false };
        };

        void finalize_active_scene_activation(core::game_context_t& game) noexcept;
        void bind_active_runtime_objects(core::game_context_t& game) noexcept;
        void bind_active_player_controller(core::game_context_t& game) noexcept;
        void bind_active_interaction_controller() noexcept;
        void apply_camera_defaults(core::game_context_t& game,
                                   const assets::scene_asset_t& scene,
                                   const scene_camera_override_t& override) noexcept;
        void center_camera_on_initial_target(core::game_context_t& game) noexcept;
        void refresh_scene_music(const assets::scene_asset_t& scene) noexcept;
        void capture_recent_transition_diagnostics(
            scene_change_outcome_t outcome = scene_change_outcome_t::in_progress) noexcept;
        void render_transition_diagnostics(core::game_context_t& game) noexcept;
        [[nodiscard]] bool request_scene_change(core::game_context_t& game,
                                               const assets::scene_asset_record_t& scene_record,
                                               std::string_view scene_id,
                                               std::string_view spawn_marker,
                                               const scene_load_options_t& options,
                                               scene_change_request_kind_t request_kind);
        void begin_scene_change(const assets::scene_asset_record_t& scene_record,
                                std::string_view scene_id,
                                std::string_view spawn_marker) noexcept;
        void adopt_pending_scene(core::game_context_t& game, world::world_t staged_world) noexcept;
        void apply_active_scene_defaults(core::game_context_t& game) noexcept;
        void notify_scene_change_complete(core::game_context_t& game);
        void fail_scene_change() noexcept;
        void complete_scene_change() noexcept;
        [[nodiscard]] bool can_activate_scene_change() const noexcept;
        [[nodiscard]] bool can_accept_scene_change_request() const noexcept;
        std::string _current_scene_id;
        std::string _current_spawn_marker;
        const assets::scene_asset_record_t* _current_scene_record{ nullptr };
        std::string _pending_scene_id;
        std::string _pending_spawn_marker;
        const assets::scene_asset_record_t* _pending_scene_record{ nullptr };
        scene_load_options_t _active_options{ };
        scene_load_options_t _pending_options{ };
        std::optional<world::scene_load_task_t> _pending_load_task;
        scene_change_request_kind_t _pending_request_kind{ scene_change_request_kind_t::none };
        scene_runtime_state_t _runtime_state{ scene_runtime_state_t::idle };
        scene_transition_phase_t _transition_phase{ scene_transition_phase_t::none };
        bool _last_scene_change_succeeded{ false };
        world::player_controller_t* _player_controller{ nullptr };
        world::interaction_controller_t* _interaction_controller{ nullptr };
        scene_camera_options_t _engine_camera_options{ make_default_scene_camera_options() };
        scene_camera_options_t _camera_options{ make_default_scene_camera_options() };
        scene_camera_options_t _active_camera_options{ make_default_scene_camera_options() };
        audio::voice_handle_t _music_handle{ audio::voice_handle_t::invalid() };
        scene_transition_overlay_options_t _engine_transition_overlay_options{ make_default_transition_overlay_options() };
        scene_transition_overlay_options_t _transition_overlay_options{ make_default_transition_overlay_options() };
        scene_transition_overlay_options_t _active_transition_overlay_options{ };
        transition_overlay_stage_t _transition_overlay_stage{ transition_overlay_stage_t::hidden };
        float _transition_overlay_opacity{ 0.f };
        float _transition_overlay_hold_elapsed_seconds{ 0.f };
        bool _startup_overlay_waiting_for_first_present{ false };
        recent_transition_diagnostics_t _recent_transition_diagnostics{ };
        float _transition_diagnostics_hold_remaining_seconds{ 0.f };
    };

    [[nodiscard]] bool load(core::game_context_t& game,
                            scene_runtime_t& runtime,
                            std::string_view scene_id,
                            const scene_load_options_t& options = {});
    [[nodiscard]] bool transition(core::game_context_t& game,
                                  scene_runtime_t& runtime,
                                  const scene_transition_request_t& request,
                                  const scene_load_options_t& options = {});
} // namespace carrot::scene
