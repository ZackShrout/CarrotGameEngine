#pragma once

#include "World/Controllers/PlayerController.h"
#include "World/Motion.h"
#include "World/WorldObject.h"

#include <vector>

namespace carrot::world {
    class patrol_npc_controller_t
    {
    public:
        enum class route_mode_t : uint8_t
        {
            loop = 0,
            once,
            ping_pong
        };

        void set_controlled_object(world_object_t* object) noexcept;
        void set_animation_set(player_controller_animation_set_t animation_set);
        void set_move_speed(float units_per_second) noexcept { _move_speed = units_per_second; }
        [[nodiscard]] float move_speed() const noexcept { return _move_speed; }
        void set_pause_duration(float seconds) noexcept;
        [[nodiscard]] float pause_duration() const noexcept { return _pause_duration; }
        void set_route_mode(route_mode_t mode) noexcept;
        void set_route_points(std::vector<chlm::float2> route_points_world);
        [[nodiscard]] const std::vector<chlm::float2>& route_points() const noexcept { return _route_points_world; }
        [[nodiscard]] route_mode_t route_mode() const noexcept { return _route_mode; }
        [[nodiscard]] world_object_t* controlled_object() noexcept { return _controlled_object; }
        [[nodiscard]] const world_object_t* controlled_object() const noexcept { return _controlled_object; }
        [[nodiscard]] const movement_intent_t& movement_intent() const noexcept { return _movement_intent; }
        [[nodiscard]] const movement_result_t& last_move_result() const noexcept { return _last_move_result; }
        void update(world_t& world, float delta_time);

    private:
        void apply_animation(world_object_t& object, facing_direction_t facing, bool moving);
        void advance_route_target() noexcept;

        world_object_t* _controlled_object{ nullptr };
        movement_body_t _movement_body{ };
        top_down_movement_motor_t _movement_motor{ };
        player_controller_animation_set_t _animation_set{ };
        std::vector<chlm::float2> _route_points_world;
        movement_intent_t _movement_intent{ };
        movement_result_t _last_move_result{ };
        size_t _target_index{ 0u };
        int _route_direction{ 1 };
        bool _route_finished{ false };
        route_mode_t _route_mode{ route_mode_t::loop };
        float _move_speed{ 2.0f };
        float _pause_duration{ 0.f };
        float _pause_remaining{ 0.f };
        bool _skip_next_pause{ true };
        float _waypoint_reached_distance{ 0.1f };
        facing_direction_t _facing_direction{ facing_direction_t::down };
        std::string _current_animation{ "idle_down" };
    };
} // namespace carrot::world
