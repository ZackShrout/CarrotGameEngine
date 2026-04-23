//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/GameContext.h"
#include "World/Motion.h"
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

    using player_move_result_t = movement_result_t;

    class player_controller_t
    {
    public:
        virtual ~player_controller_t() = default;

        void set_controlled_object(world_object_t* object) noexcept;

        [[nodiscard]] world_object_t* controlled_object() noexcept { return _controlled_object; }
        [[nodiscard]] const world_object_t* controlled_object() const noexcept { return _controlled_object; }
        [[nodiscard]] bool has_controlled_object() const noexcept { return _controlled_object != nullptr; }
        [[nodiscard]] const movement_body_t& movement_body() const noexcept { return _movement_body; }

        void set_movement_intent(movement_intent_t intent) noexcept { _movement_intent = intent; }
        void set_move_intent(chlm::float2 intent) noexcept { set_movement_intent(movement_intent_t{ .move_direction = intent }); }
        [[nodiscard]] const movement_intent_t& movement_intent() const noexcept { return _movement_intent; }
        [[nodiscard]] chlm::float2 move_intent() const noexcept { return _movement_intent.move_direction; }
        void clear_movement_intent() noexcept;

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

        movement_intent_t _movement_intent{ };
        float _move_speed{ 4.0f };
        facing_direction_t _facing_direction{ facing_direction_t::down };
        std::string _current_animation{ "idle_down" };
        player_controller_animation_set_t _animation_set{ };
        movement_body_t _movement_body{ };
        top_down_movement_motor_t _movement_motor{ };
        player_move_result_t _last_move_result{ };
    };
} // namespace carrot::world
