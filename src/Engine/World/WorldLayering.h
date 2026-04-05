//
// Created by Codex on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Tilemap/TilemapAsset.h"
#include "Renderer/Draw/RenderLayer.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace carrot::world {
    enum class layer_visibility_rule_t : std::uint8_t
    {
        always = 0,
        hidden_when_tag_active,
        visible_when_tag_active
    };

    struct authored_layer_semantics_t
    {
        renderer::render_layer_t render_layer{ renderer::render_layer_t::world_back };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        std::string visibility_tag;
        layer_visibility_rule_t visibility_rule{ layer_visibility_rule_t::always };
    };

    [[nodiscard]] authored_layer_semantics_t resolve_tile_layer_semantics(const assets::tilemap_layer_t& layer,
                                                                          int32_t source_layer_index) noexcept;
    [[nodiscard]] authored_layer_semantics_t resolve_object_layer_semantics(const assets::tilemap_layer_t& layer,
                                                                            int32_t source_layer_index) noexcept;
    [[nodiscard]] bool is_layer_visible(const authored_layer_semantics_t& semantics,
                                        std::span<const std::string_view> active_visibility_tags) noexcept;
    [[nodiscard]] bool is_visibility_region_object(const assets::tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<std::string_view> visibility_region_tag_for_object(
        const assets::tilemap_object_t& object) noexcept;
} // namespace carrot::world
