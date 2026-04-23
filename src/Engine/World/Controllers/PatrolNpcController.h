#pragma once

#include "World/Controllers/PlayerController.h"
#include "World/Motion.h"
#include "World/WorldObject.h"

#include <vector>

namespace carrot::world {
    class patrol_npc_controller_t
    {
    public:
        void set_controlled_object(world_object_t* object) noexcept;
        void set_animation_set(player_controller_animation_set_t animation_set);
        void set_move_speed(float units_per_second) noexcept { _move_speed = units_per_second; }
        void set_route_points(std::vector<chlm::float2> route_points_world);
        [[nodiscard]] const std::vector<chlm::float2>& route_points() const noexcept { return _route_points_world; }
        [[nodiscard]] world_object_t* controlled_object() noexcept { return _controlled_object; }
        [[nodiscard]] const world_object_t* controlled_object() const noexcept { return _controlled_object; }
        [[nodiscard]] const movement_intent_t& movement_intent() const noexcept { return _movement_intent; }
        [[nodiscard]] const movement_result_t& last_move_result() const noexcept { return _last_move_result; }
        void update(world_t& world, float delta_time);

    private:
        void apply_animation(world_object_t& object, facing_direction_t facing, bool moving);

        world_object_t* _controlled_object{ nullptr };
        movement_body_t _movement_body{ };
        top_down_movement_motor_t _movement_motor{ };
        player_controller_animation_set_t _animation_set{ };
        std::vector<chlm::float2> _route_points_world;
        movement_intent_t _movement_intent{ };
        movement_result_t _last_move_result{ };
        size_t _target_index{ 0u };
        float _move_speed{ 2.0f };
        float _waypoint_reached_distance{ 0.1f };
        facing_direction_t _facing_direction{ facing_direction_t::down };
        std::string _current_animation{ "idle_down" };
    };
} // namespace carrot::world
