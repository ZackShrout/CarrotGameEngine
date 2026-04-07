//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WorldLayering.h"

#include <algorithm>
#include <cctype>

namespace carrot::world {
    namespace {
        [[nodiscard]] std::string lowercase_copy(std::string_view value)
        {
            std::string result{ value };
            std::ranges::transform(result, result.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return result;
        }

        [[nodiscard]] bool equals_case_insensitive(std::string_view lhs, std::string_view rhs)
        {
            return lowercase_copy(lhs) == lowercase_copy(rhs);
        }

        [[nodiscard]] std::optional<renderer::render_layer_t> parse_render_layer(std::string_view value) noexcept
        {
            if (value == "background") return renderer::render_layer_t::background;
            if (value == "world_back") return renderer::render_layer_t::world_back;
            if (value == "actors") return renderer::render_layer_t::actors;
            if (value == "world_front") return renderer::render_layer_t::world_front;
            if (value == "effects") return renderer::render_layer_t::effects;
            if (value == "debug") return renderer::render_layer_t::debug;
            if (value == "ui") return renderer::render_layer_t::ui;
            return std::nullopt;
        }

        [[nodiscard]] std::optional<renderer::render_order_mode_t> parse_order_mode(std::string_view value) noexcept
        {
            if (value == "explicit" || value == "explicit_order")
                return renderer::render_order_mode_t::explicit_order;
            if (value == "anchor_bottom_y" || value == "bottom_y")
                return renderer::render_order_mode_t::anchor_bottom_y;
            return std::nullopt;
        }

        [[nodiscard]] std::optional<layer_visibility_rule_t> parse_visibility_rule(std::string_view value) noexcept
        {
            if (value == "always")
                return layer_visibility_rule_t::always;
            if (value == "hidden_when_tag_active" || value == "hide_when_tag_active")
                return layer_visibility_rule_t::hidden_when_tag_active;
            if (value == "visible_when_tag_active" || value == "show_when_tag_active")
                return layer_visibility_rule_t::visible_when_tag_active;
            return std::nullopt;
        }

        void apply_authored_overrides(const assets::tilemap_layer_t& layer, authored_layer_semantics_t& semantics) noexcept
        {
            if (const auto render_layer{ layer.get_string_property("carrot_render_layer") })
            {
                if (const auto parsed{ parse_render_layer(*render_layer) })
                    semantics.render_layer = *parsed;
            }

            if (const auto order_mode{ layer.get_string_property("carrot_order_mode") })
            {
                if (const auto parsed{ parse_order_mode(*order_mode) })
                    semantics.order_mode = *parsed;
            }

            if (const auto order_in_layer{ layer.get_number_property("carrot_order") })
                semantics.order_in_layer = static_cast<int32_t>(*order_in_layer);

            if (const auto visibility_tag{ layer.get_string_property("carrot_visibility_tag") })
            {
                semantics.visibility_tag = std::string{ *visibility_tag };
                if (semantics.visibility_rule == layer_visibility_rule_t::always)
                    semantics.visibility_rule = layer_visibility_rule_t::hidden_when_tag_active;
            }
            else if (const auto visibility_zone{ layer.get_string_property("carrot_visibility_zone") })
            {
                semantics.visibility_tag = std::string{ *visibility_zone };
                if (semantics.visibility_rule == layer_visibility_rule_t::always)
                    semantics.visibility_rule = layer_visibility_rule_t::hidden_when_tag_active;
            }
            else if (const auto visibility_zone_id{ layer.get_string_property("visibility_zone_id") })
            {
                semantics.visibility_tag = std::string{ *visibility_zone_id };
                if (semantics.visibility_rule == layer_visibility_rule_t::always)
                    semantics.visibility_rule = layer_visibility_rule_t::hidden_when_tag_active;
            }

            if (const auto visibility_rule{ layer.get_string_property("carrot_visibility_rule") })
            {
                if (const auto parsed{ parse_visibility_rule(*visibility_rule) })
                    semantics.visibility_rule = *parsed;
            }

            const bool conditional_front{ layer.get_bool_property("carrot_conditional_front").value_or(false) };
            const bool always_front{ layer.get_bool_property("carrot_always_front").value_or(false) };

            if (always_front)
            {
                semantics.render_layer = renderer::render_layer_t::world_front;
                semantics.order_mode = renderer::render_order_mode_t::explicit_order;
            }
            else if (conditional_front)
            {
                semantics.render_layer = renderer::render_layer_t::actors;
                semantics.order_mode = renderer::render_order_mode_t::anchor_bottom_y;
            }
        }

        [[nodiscard]] authored_layer_semantics_t default_tile_layer_semantics(const assets::tilemap_layer_t& layer,
                                                                              const int32_t source_layer_index) noexcept
        {
            authored_layer_semantics_t semantics{ };
            semantics.order_in_layer = source_layer_index * 10;

            if (equals_case_insensitive(layer.authored_type, "Water"))
            {
                semantics.render_layer = renderer::render_layer_t::background;
                return semantics;
            }

            if (equals_case_insensitive(layer.authored_type, "Roof"))
            {
                semantics.render_layer = renderer::render_layer_t::world_front;
                return semantics;
            }

            return semantics;
        }
    } // namespace

    authored_layer_semantics_t resolve_tile_layer_semantics(const assets::tilemap_layer_t& layer,
                                                            const int32_t source_layer_index) noexcept
    {
        authored_layer_semantics_t semantics{ default_tile_layer_semantics(layer, source_layer_index) };
        apply_authored_overrides(layer, semantics);
        return semantics;
    }

    authored_layer_semantics_t resolve_object_layer_semantics(const assets::tilemap_layer_t& layer,
                                                              const int32_t source_layer_index) noexcept
    {
        authored_layer_semantics_t semantics{ };
        semantics.render_layer = renderer::render_layer_t::actors;
        semantics.order_mode = renderer::render_order_mode_t::anchor_bottom_y;
        semantics.order_in_layer = source_layer_index * 10;

        apply_authored_overrides(layer, semantics);
        return semantics;
    }

    bool is_layer_visible(const authored_layer_semantics_t& semantics,
                          const std::span<const std::string_view> active_visibility_tags) noexcept
    {
        if (semantics.visibility_rule == layer_visibility_rule_t::always || semantics.visibility_tag.empty())
            return true;

        const bool tag_is_active{
            std::ranges::find(active_visibility_tags, std::string_view{ semantics.visibility_tag }) != active_visibility_tags.end()
        };

        switch (semantics.visibility_rule)
        {
            case layer_visibility_rule_t::always:
                return true;
            case layer_visibility_rule_t::hidden_when_tag_active:
                return !tag_is_active;
            case layer_visibility_rule_t::visible_when_tag_active:
                return tag_is_active;
        }

        return true;
    }

    bool is_visibility_region_object(const assets::tilemap_object_t& object) noexcept
    {
        return object.type == "VisibilityZone";
    }

    std::optional<std::string_view> visibility_region_tag_for_object(const assets::tilemap_object_t& object) noexcept
    {
        if (const auto explicit_tag{ object.get_string_property("visibility_tag") })
            return explicit_tag;

        if (const auto explicit_zone{ object.get_string_property("visibility_zone_id") })
            return explicit_zone;

        return std::nullopt;
    }
} // namespace carrot::world
