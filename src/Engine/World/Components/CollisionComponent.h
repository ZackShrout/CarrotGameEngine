//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace carrot::world {
    enum class collision_participation_kind_t : uint8_t
    {
        dynamic_body = 0,
        trigger_volume
    };

    [[nodiscard]] constexpr bool collision_participates_in_movement_queries(
        const collision_participation_kind_t kind) noexcept
    {
        return kind == collision_participation_kind_t::dynamic_body;
    }

    [[nodiscard]] constexpr bool collision_participates_in_trigger_queries(
        const collision_participation_kind_t kind) noexcept
    {
        return kind == collision_participation_kind_t::trigger_volume;
    }

    [[nodiscard]] constexpr std::string_view to_string(const collision_participation_kind_t kind) noexcept
    {
        switch (kind)
        {
            case collision_participation_kind_t::dynamic_body: return "dynamic_body";
            case collision_participation_kind_t::trigger_volume: return "trigger_volume";
        }

        return "unknown";
    }

    struct collision_debug_display_t
    {
        bool filled{ false };
        float outline_thickness{ 2.f };
        uint32_t color{ 0xFF00FF00u };
    };

    struct collision_component_t
    {
        collision_participation_kind_t participation{ collision_participation_kind_t::dynamic_body };
        chlm::float2 half_extents{ 0.3f, 0.2f };
        chlm::float2 offset{ 0.f, -0.2f };
        std::optional<collision_debug_display_t> debug_display;
    };
} // namespace carrot::world
