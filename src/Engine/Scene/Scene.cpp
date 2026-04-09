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
#include "World/Controllers/InteractionController.h"
#include "World/Controllers/PlayerController.h"
#include "World/SceneLoader.h"
#include "World/World.h"

namespace carrot::scene {
    namespace {
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
    } // namespace

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
        world::world_t staged_world;
        if (!world::scene_loader_t::load_scene(staged_world,
                                               game.assets,
                                               scene_id,
                                               options.spawn_marker_override))
        {
            return false;
        }

        if (options.validate_loaded_scene &&
            !options.validate_loaded_scene(game.assets, staged_world, scene_id))
        {
            return false;
        }

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

        if (options.listener)
            options.listener->before_scene_change(game,
                                                 had_scene_loaded ? &previous_context : nullptr,
                                                 scene_id,
                                                 resolved_spawn_marker);

        game.world = std::move(staged_world);

        _player_controller = options.player_controller;
        _interaction_controller = options.interaction_controller;
        _current_scene_record = scene_record;
        _current_scene_id = std::string{ scene_id };
        _current_spawn_marker = resolved_spawn_marker;

        bind_runtime_objects(game, scene_record->scene);

        if (options.apply_camera_defaults)
        {
            apply_camera_defaults(game, scene_record->scene);
            center_camera_on_initial_target(game);
        }

        if (options.apply_scene_music)
            refresh_scene_music(scene_record->scene);

        if (options.listener)
        {
            const scene_runtime_context_t current_context{ make_context(game) };
            options.listener->after_scene_change(game, current_context);
        }

        return true;
    }

    bool scene_runtime_t::transition(core::game_context_t& game,
                                     const scene_transition_request_t& request,
                                     const scene_load_options_t& options)
    {
        scene_load_options_t transition_options{ options };
        transition_options.spawn_marker_override = request.marker_name;
        return load(game, request.scene_id, transition_options);
    }

    void scene_runtime_t::update_camera_follow(core::game_context_t& game, const float delta_time) noexcept
    {
        if (_camera_follow_mode != assets::scene_camera_follow_mode_t::player || !_player_controller)
            return;

        if (const world::world_object_t* player{ _player_controller->controlled_object() };
            player && player->transform)
        {
            const chlm::float2 current_center{ game.view.center_world_position(game.world) };
            const chlm::float2 target_position{ player->transform->position };
            const chlm::float2 half_dead_zone{
                _camera_dead_zone_size_world.x * 0.5f,
                _camera_dead_zone_size_world.y * 0.5f
            };

            chlm::float2 desired_center{
                apply_dead_zone_axis(current_center.x, target_position.x, half_dead_zone.x),
                apply_dead_zone_axis(current_center.y, target_position.y, half_dead_zone.y)
            };

            if (_camera_follow_smoothing > 0.f && delta_time > 0.f)
            {
                const float alpha{ 1.f - std::exp(-_camera_follow_smoothing * delta_time) };
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

    void scene_runtime_t::apply_camera_defaults(core::game_context_t& game, const assets::scene_asset_t& scene) noexcept
    {
        game.view.set_zoom(scene.camera.zoom);
        _camera_follow_mode = scene.camera.follow_mode;
        _camera_initial_target_policy = scene.camera.initial_target_policy;
        _camera_dead_zone_size_world = {
            std::max(0.f, scene.camera.dead_zone_size_world.x),
            std::max(0.f, scene.camera.dead_zone_size_world.y)
        };
        _camera_follow_smoothing = std::max(0.f, scene.camera.follow_smoothing);
    }

    void scene_runtime_t::center_camera_on_initial_target(core::game_context_t& game) noexcept
    {
        if (_camera_initial_target_policy == assets::scene_camera_initial_target_policy_t::spawn_marker)
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
