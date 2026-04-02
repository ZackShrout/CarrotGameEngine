//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Tilemap/TilemapAsset.h"
#include "Components/SpriteAnimatorComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TileObjectComponent.h"
#include "Components/TilemapComponent.h"
#include "Components/TransformComponent.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::world {
    using world_object_id_t = uint64_t;

    struct world_object_source_t
    {
        std::string tilemap_logical_id;
        std::string layer_name;
        uint32_t object_id{ 0 };
        std::string object_name;
    };

    class world_object_t
    {
    public:
        world_object_id_t id{ 0 };
        std::string name;
        std::string type;
        std::optional<world_object_source_t> source;
        std::vector<assets::tilemap_property_t> properties;

        [[nodiscard]] const assets::tilemap_property_t* find_property(const std::string_view property_name) const noexcept
        {
            for (const assets::tilemap_property_t& property : properties)
            {
                if (property.name == property_name)
                    return &property;
            }

            return nullptr;
        }

        [[nodiscard]] bool has_property(const std::string_view property_name) const noexcept
        {
            return find_property(property_name) != nullptr;
        }

        [[nodiscard]] std::optional<std::string_view> get_string_property(const std::string_view property_name) const noexcept
        {
            const assets::tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const std::string* value{ std::get_if<std::string>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<bool> get_bool_property(const std::string_view property_name) const noexcept
        {
            const assets::tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const bool* value{ std::get_if<bool>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> get_number_property(const std::string_view property_name) const noexcept
        {
            const assets::tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const double* value{ std::get_if<double>(&property->value) })
                return *value;

            return std::nullopt;
        }

        std::optional<transform_component_t> transform;
        std::optional<sprite_component_t> sprite;
        std::optional<sprite_animator_component_t> sprite_animator;
        std::optional<tile_object_component_t> tile_object;
        std::optional<tilemap_component_t> tilemap;
    };
} // namespace carrot::world
