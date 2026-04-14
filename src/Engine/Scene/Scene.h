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

    [[nodiscard]] std::string_view to_string(scene_runtime_state_t state) noexcept;
    [[nodiscard]] std::string_view to_string(scene_transition_phase_t phase) noexcept;

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
        enum class transition_overlay_stage_t : uint8_t
        {
            hidden = 0,
            fading_out_to_black,
            holding_opaque,
            fading_in_from_black
        };

        struct recent_transition_diagnostics_t
        {
            scene_runtime_state_t runtime_state{ scene_runtime_state_t::idle };
            scene_transition_phase_t transition_phase{ scene_transition_phase_t::none };
            scene_transition_overlay_style_t overlay_style{ scene_transition_overlay_style_t::fade };
            scene_transition_wipe_direction_t wipe_direction{ scene_transition_wipe_direction_t::left_to_right };
            std::string active_scene_id;
            std::string pending_scene_id;
            std::string pending_spawn_marker;
            float transition_progress{ 0.f };
            bool startup_waiting_for_first_present{ false };
        };

        void bind_runtime_objects(core::game_context_t& game, const assets::scene_asset_t& scene) noexcept;
        void apply_camera_defaults(core::game_context_t& game,
                                   const assets::scene_asset_t& scene,
                                   const scene_camera_override_t& override) noexcept;
        void center_camera_on_initial_target(core::game_context_t& game) noexcept;
        void refresh_scene_music(const assets::scene_asset_t& scene) noexcept;
        void capture_recent_transition_diagnostics() noexcept;
        void render_transition_diagnostics(core::game_context_t& game) noexcept;
        void begin_scene_change(const assets::scene_asset_record_t& scene_record,
                                std::string_view scene_id,
                                std::string_view spawn_marker) noexcept;
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
        scene_load_options_t _pending_options{ };
        std::optional<world::scene_load_task_t> _pending_load_task;
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
