//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/GameContext.h"
#include "Collision/CollisionWorld.h"
#include "World/WorldObject.h"

#include <cstdint>

namespace carrot::world {
    enum class facing_direction_t : uint8_t
    {
        down = 0,
        up,
        left,
        right
    };

    struct player_controller_animation_set_t
    {
        std::string idle_down{ "idle_down" };
        std::string idle_up{ "idle_up" };
        std::string idle_left{ "idle_left" };
        std::string idle_right{ "idle_right" };

        std::string walk_down{ "walk_down" };
        std::string walk_up{ "walk_up" };
        std::string walk_left{ "walk_left" };
        std::string walk_right{ "walk_right" };
    };

    struct player_move_result_t
    {
        chlm::float2 requested_delta{ 0.f, 0.f };
        chlm::float2 actual_delta{ 0.f, 0.f };
        bool blocked_x{ false };
        bool blocked_y{ false };
        bool started_overlapping{ false };
    };

    class player_controller_t
    {
    public:
        virtual ~player_controller_t() = default;

        void set_controlled_object(world_object_t* object) noexcept;

        [[nodiscard]] world_object_t* controlled_object() noexcept { return _controlled_object; }
        [[nodiscard]] const world_object_t* controlled_object() const noexcept { return _controlled_object; }

        void set_move_input(bool up, bool down, bool left, bool right) noexcept;
        void set_move_up(bool value) noexcept { _move_up = value; }
        void set_move_down(bool value) noexcept { _move_down = value; }
        void set_move_left(bool value) noexcept { _move_left = value; }
        void set_move_right(bool value) noexcept { _move_right = value; }

        void set_move_speed(float units_per_second) noexcept { _move_speed = units_per_second; }
        [[nodiscard]] float move_speed() const noexcept { return _move_speed; }

        void set_animation_set(player_controller_animation_set_t animation_set);
        [[nodiscard]] const player_controller_animation_set_t& animation_set() const noexcept { return _animation_set; }

        [[nodiscard]] facing_direction_t facing_direction() const noexcept { return _facing_direction; }
        void set_facing_direction(facing_direction_t direction);

        void update(core::game_context_t& game, float delta_time);
        [[nodiscard]] player_move_result_t update(world_t& world, float delta_time);
        [[nodiscard]] player_move_result_t move(world_t& world, chlm::float2 delta);
        [[nodiscard]] const player_move_result_t& last_move_result() const noexcept { return _last_move_result; }
        [[nodiscard]] collision::collision_aabb_t collision_bounds_at(chlm::float2 position) const noexcept;
        [[nodiscard]] collision::collision_aabb_t current_collision_bounds() const noexcept;

    protected:
        virtual void apply_animation(world_object_t& object, facing_direction_t facing, bool moving);

    private:
        world_object_t* _controlled_object{ nullptr };

        bool _move_up{ false };
        bool _move_down{ false };
        bool _move_left{ false };
        bool _move_right{ false };

        float _move_speed{ 4.0f };
        facing_direction_t _facing_direction{ facing_direction_t::down };
        std::string _current_animation{ "idle_down" };
        player_controller_animation_set_t _animation_set{ };
        player_move_result_t _last_move_result{ };
    };
} // namespace carrot::world
