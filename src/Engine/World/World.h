//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Collision/CollisionWorld.h"
#include "Controllers/PatrolNpcController.h"
#include "WorldObject.h"
#include "WorldUnits.h"

#include <string_view>
#include <vector>

namespace carrot::world {
    struct collision_debug_view_t
    {
        bool show_map_collision{ false };
        bool show_object_colliders{ false };
        bool show_trigger_volumes{ false };
        uint32_t map_collision_color{ 0xFF00FF00u };
        float map_outline_thickness{ 2.f };
        uint32_t trigger_volume_color{ 0xFFFF00FFu };
        float trigger_outline_thickness{ 2.f };
        bool trigger_filled{ true };
    };

    struct layering_debug_view_t
    {
        bool show_visibility_regions{ false };
        uint32_t visibility_region_color{ 0x6633AAFFu };
    };

    struct layering_debug_layer_snapshot_t
    {
        std::string layer_name;
        std::string source_kind;
        renderer::render_layer_t resolved_render_layer{ renderer::render_layer_t::world_back };
        renderer::render_order_mode_t resolved_order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t resolved_order_in_layer{ 0 };
        std::string visibility_zone_id;
        bool is_visible{ true };
        bool hidden_by_visibility_zone{ false };
        bool is_conditional_front{ false };
        bool is_always_front{ false };
    };

    struct layering_debug_snapshot_t
    {
        uint64_t frame_index{ 0 };
        bool has_visibility_anchor{ false };
        chlm::float2 visibility_anchor_world{ 0.f, 0.f };
        uint32_t visibility_region_count{ 0 };
        uint32_t rendered_tilemap_count{ 0 };
        uint32_t layer_count{ 0 };
        uint32_t visible_layer_count{ 0 };
        uint32_t hidden_layer_count{ 0 };
        uint32_t visibility_bound_layer_count{ 0 };
        uint32_t conditional_front_layer_count{ 0 };
        uint32_t always_front_layer_count{ 0 };
        std::vector<std::string> active_visibility_tags;
        std::vector<layering_debug_layer_snapshot_t> layers;
    };

    struct world_lighting_state_t
    {
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };

        struct point_light_t
        {
            enum class runtime_behavior_t : uint8_t
            {
                stationary = 0,
                follow_object
            };

            chlm::float2 position_world{ 0.f, 0.f };
            float radius_world{ 2.f };
            float reserved0{ 0.f };
            chlm::float4 color{ 1.f, 1.f, 1.f, 1.f };
            float intensity{ 1.f };
            runtime_behavior_t behavior{ runtime_behavior_t::stationary };
            chlm::float2 follow_offset_world{ 0.f, 0.f };
            std::string follow_object_name;
        };

        std::vector<point_light_t> point_lights;
    };

    class world_t
    {
    public:
        [[nodiscard]] world_object_t& create_object();
        [[nodiscard]] world_object_t* find_object_by_name(std::string_view name) noexcept;
        [[nodiscard]] const world_object_t* find_object_by_name(std::string_view name) const noexcept;
        [[nodiscard]] world_object_t* find_first_object_by_type(std::string_view type) noexcept;
        [[nodiscard]] const world_object_t* find_first_object_by_type(std::string_view type) const noexcept;
        [[nodiscard]] std::vector<world_object_t*> find_objects_by_type(std::string_view type) noexcept;
        [[nodiscard]] std::vector<const world_object_t*> find_objects_by_type(std::string_view type) const;
        [[nodiscard]] world_object_t* find_nearest_object_by_type(std::string_view type,
                                                                  const chlm::float2& origin,
                                                                  float max_distance) noexcept;
        [[nodiscard]] const world_object_t* find_nearest_object_by_type(std::string_view type,
                                                                        const chlm::float2& origin,
                                                                        float max_distance) const noexcept;
        [[nodiscard]] const std::vector<world_object_t>& objects() const noexcept { return _objects; }
        [[nodiscard]] std::vector<world_object_t>& objects() noexcept { return _objects; }
        [[nodiscard]] std::vector<patrol_npc_controller_t>& patrol_npc_controllers() noexcept { return _patrol_npc_controllers; }
        [[nodiscard]] const std::vector<patrol_npc_controller_t>& patrol_npc_controllers() const noexcept
        {
            return _patrol_npc_controllers;
        }
        void clear() noexcept;
        void update(float delta_time) noexcept;
        void refresh_bound_lights() noexcept;
        void set_presentation_origin_px(const chlm::float2 origin) noexcept { _presentation.origin_px = origin; }
        [[nodiscard]] const chlm::float2& presentation_origin_px() const noexcept { return _presentation.origin_px; }
        void set_presentation_pixels_per_unit(const float pixels_per_unit) noexcept
        {
            _presentation.set_pixels_per_unit(pixels_per_unit);
        }
        [[nodiscard]] float presentation_pixels_per_unit() const noexcept { return _presentation.pixels_per_unit; }
        [[nodiscard]] const world_presentation_t& presentation() const noexcept { return _presentation; }
        [[nodiscard]] world_presentation_t& presentation() noexcept { return _presentation; }
        [[nodiscard]] const collision::collision_world_t& collision_world() const noexcept { return _collision_world; }
        [[nodiscard]] collision::collision_world_t& collision_world() noexcept { return _collision_world; }
        [[nodiscard]] const collision_debug_view_t& collision_debug_view() const noexcept { return _collision_debug_view; }
        [[nodiscard]] collision_debug_view_t& collision_debug_view() noexcept { return _collision_debug_view; }
        [[nodiscard]] const layering_debug_view_t& layering_debug_view() const noexcept { return _layering_debug_view; }
        [[nodiscard]] layering_debug_view_t& layering_debug_view() noexcept { return _layering_debug_view; }
        [[nodiscard]] const world_lighting_state_t& lighting() const noexcept { return _lighting; }
        [[nodiscard]] world_lighting_state_t& lighting() noexcept { return _lighting; }
        [[nodiscard]] const layering_debug_snapshot_t& layering_debug_snapshot() const noexcept
        {
            return _layering_debug_snapshot;
        }
        void set_layering_debug_snapshot(layering_debug_snapshot_t snapshot) const noexcept;
        [[nodiscard]] std::vector<std::string_view> collect_active_visibility_tags(const chlm::float2& point) const;

    private:
        void sync_follow_lights() noexcept;

        world_object_id_t _next_id{ 1 };
        std::vector<world_object_t> _objects;
        std::vector<patrol_npc_controller_t> _patrol_npc_controllers;
        world_presentation_t _presentation{ };
        collision::collision_world_t _collision_world{ };
        collision_debug_view_t _collision_debug_view{ };
        layering_debug_view_t _layering_debug_view{ };
        world_lighting_state_t _lighting{ };
        mutable layering_debug_snapshot_t _layering_debug_snapshot{ };
    };
} // namespace carrot::world
