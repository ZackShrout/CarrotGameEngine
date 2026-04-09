//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Scene/SceneAsset.h"
#include "Audio/Voice/VoiceHandle.h"
#include "World/WorldObject.h"

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

    class scene_runtime_t
    {
    public:
        [[nodiscard]] bool load(core::game_context_t& game,
                                std::string_view scene_id,
                                const scene_load_options_t& options = {});
        [[nodiscard]] bool transition(core::game_context_t& game,
                                      const scene_transition_request_t& request,
                                      const scene_load_options_t& options = {});
        void update_camera_follow(core::game_context_t& game, float delta_time) noexcept;

        [[nodiscard]] std::string_view current_scene_id() const noexcept { return _current_scene_id; }
        [[nodiscard]] std::string_view current_spawn_marker() const noexcept { return _current_spawn_marker; }
        [[nodiscard]] bool has_scene_loaded() const noexcept { return !_current_scene_id.empty(); }
        [[nodiscard]] scene_runtime_context_t make_context(core::game_context_t& game) const noexcept;

    private:
        void bind_runtime_objects(core::game_context_t& game, const assets::scene_asset_t& scene) noexcept;
        void apply_camera_defaults(core::game_context_t& game, const assets::scene_asset_t& scene) noexcept;
        void center_camera_on_initial_target(core::game_context_t& game) noexcept;
        void refresh_scene_music(const assets::scene_asset_t& scene) noexcept;

        std::string _current_scene_id;
        std::string _current_spawn_marker;
        const assets::scene_asset_record_t* _current_scene_record{ nullptr };
        world::player_controller_t* _player_controller{ nullptr };
        world::interaction_controller_t* _interaction_controller{ nullptr };
        assets::scene_camera_follow_mode_t _camera_follow_mode{
            assets::scene_camera_follow_mode_t::player
        };
        assets::scene_camera_initial_target_policy_t _camera_initial_target_policy{
            assets::scene_camera_initial_target_policy_t::player
        };
        chlm::float2 _camera_dead_zone_size_world{ 0.f, 0.f };
        float _camera_follow_smoothing{ 0.f };
        audio::voice_handle_t _music_handle{ audio::voice_handle_t::invalid() };
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
