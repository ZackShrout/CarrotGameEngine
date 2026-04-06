//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"

#include <chlm/CarrotHLM.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace carrot::assets {
    enum class tilemap_orientation_t : uint8_t
    {
        orthogonal
    };

    enum class tilemap_layer_kind_t : uint8_t
    {
        tile,
        object
    };

    using tilemap_property_value_t = std::variant<bool, double, std::string>;

    enum class tilemap_validation_issue_severity_t : uint8_t
    {
        warning = 0,
        error
    };

    struct tilemap_validation_issue_t
    {
        tilemap_validation_issue_severity_t severity{ tilemap_validation_issue_severity_t::warning };
        std::string code;
        std::string message;
    };

    struct tilemap_property_t
    {
        std::string name;
        tilemap_property_value_t value;
    };

    [[nodiscard]] inline const tilemap_property_t* find_tilemap_property(
        const std::vector<tilemap_property_t>& properties,
        const std::string_view property_name) noexcept
    {
        for (const tilemap_property_t& property : properties)
        {
            if (property.name == property_name)
                return &property;
        }

        return nullptr;
    }

    struct tilemap_tileset_t
    {
        struct animation_frame_t
        {
            uint32_t tile_id{ 0 };
            uint32_t duration_ms{ 0 };
        };

        struct tile_animation_t
        {
            uint32_t tile_id{ 0 };
            uint32_t total_duration_ms{ 0 };
            std::vector<animation_frame_t> frames;

            void rebuild_cached_duration() noexcept
            {
                total_duration_ms = 0;
                for (const animation_frame_t& frame : frames)
                    total_duration_ms += frame.duration_ms;
            }
        };

        struct collision_rect_t
        {
            float x{ 0.f };
            float y{ 0.f };
            float width{ 0.f };
            float height{ 0.f };
        };

        struct tile_collision_t
        {
            uint32_t tile_id{ 0 };
            std::vector<collision_rect_t> collision_rects;
        };

        std::string name;
        uint32_t first_gid{ 0 };

        // When Tiled references an external TSX, this remains populated even if
        // the importer does not yet expand it into full engine data.
        std::string source_uri;
        std::string image_source_uri;

        uint32_t tile_width{ 0 };
        uint32_t tile_height{ 0 };
        uint32_t image_width{ 0 };
        uint32_t image_height{ 0 };
        uint32_t tile_count{ 0 };
        uint32_t columns{ 0 };
        std::vector<tile_animation_t> tile_animations;
        std::vector<tile_collision_t> tile_collisions;
        std::vector<int32_t> animation_lookup_by_tile_id;

        [[nodiscard]] const tile_collision_t* find_tile_collision(const uint32_t tile_id) const noexcept
        {
            for (const tile_collision_t& collision : tile_collisions)
            {
                if (collision.tile_id == tile_id)
                    return &collision;
            }

            return nullptr;
        }

        void rebuild_animation_lookup() noexcept
        {
            animation_lookup_by_tile_id.assign(tile_count, -1);

            for (size_t animation_index{ 0 }; animation_index < tile_animations.size(); ++animation_index)
            {
                tile_animation_t& animation{ tile_animations[animation_index] };
                animation.rebuild_cached_duration();

                if (animation.tile_id >= animation_lookup_by_tile_id.size())
                    continue;

                animation_lookup_by_tile_id[animation.tile_id] = static_cast<int32_t>(animation_index);
            }
        }

        [[nodiscard]] const tile_animation_t* find_tile_animation(const uint32_t tile_id) const noexcept
        {
            if (tile_id >= animation_lookup_by_tile_id.size())
                return nullptr;

            const int32_t animation_index{ animation_lookup_by_tile_id[tile_id] };
            if (animation_index < 0 || static_cast<size_t>(animation_index) >= tile_animations.size())
                return nullptr;

            return &tile_animations[static_cast<size_t>(animation_index)];
        }

        [[nodiscard]] uint32_t resolve_animated_tile_id(const uint32_t tile_id, const uint64_t elapsed_ms) const noexcept
        {
            const tile_animation_t* animation{ find_tile_animation(tile_id) };
            if (!animation || animation->frames.empty() || animation->total_duration_ms == 0)
                return tile_id;

            const uint64_t animation_time_ms{ elapsed_ms % animation->total_duration_ms };
            uint64_t accumulated_ms{ 0 };

            for (const animation_frame_t& frame : animation->frames)
            {
                accumulated_ms += frame.duration_ms;
                if (animation_time_ms < accumulated_ms)
                    return frame.tile_id;
            }

            return animation->frames.back().tile_id;
        }
    };

    struct tilemap_object_t
    {
        enum class geometry_kind_t : uint8_t
        {
            rectangle = 0,
            point,
            ellipse,
            polygon,
            polyline,
            text
        };

        uint32_t id{ 0 };
        std::string name;
        std::string type;
        geometry_kind_t geometry_kind{ geometry_kind_t::rectangle };
        float x{ 0.f };
        float y{ 0.f };
        float width{ 0.f };
        float height{ 0.f };
        float rotation{ 0.f };
        bool visible{ true };
        uint32_t gid{ 0 };
        std::vector<chlm::float2> geometry_points;
        std::vector<tilemap_property_t> properties;

        [[nodiscard]] const tilemap_property_t* find_property(const std::string_view property_name) const noexcept
        {
            for (const tilemap_property_t& property : properties)
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
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const std::string* value{ std::get_if<std::string>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<bool> get_bool_property(const std::string_view property_name) const noexcept
        {
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const bool* value{ std::get_if<bool>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> get_number_property(const std::string_view property_name) const noexcept
        {
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const double* value{ std::get_if<double>(&property->value) })
                return *value;

            return std::nullopt;
        }
    };

    struct tilemap_layer_t
    {
        tilemap_layer_kind_t kind{ tilemap_layer_kind_t::tile };
        std::string name;
        std::string authored_type;
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        bool visible{ true };
        float opacity{ 1.f };

        // Tile layers populate gids. Object layers populate objects.
        std::vector<uint32_t> gids;
        std::vector<tilemap_object_t> objects;
        std::vector<tilemap_property_t> properties;

        [[nodiscard]] const tilemap_property_t* find_property(const std::string_view property_name) const noexcept
        {
            for (const tilemap_property_t& property : properties)
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
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const std::string* value{ std::get_if<std::string>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<bool> get_bool_property(const std::string_view property_name) const noexcept
        {
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const bool* value{ std::get_if<bool>(&property->value) })
                return *value;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> get_number_property(const std::string_view property_name) const noexcept
        {
            const tilemap_property_t* property{ find_property(property_name) };
            if (!property)
                return std::nullopt;

            if (const double* value{ std::get_if<double>(&property->value) })
                return *value;

            return std::nullopt;
        }
    };

    class tilemap_asset_t
    {
    public:
        [[nodiscard]] tilemap_orientation_t orientation() const noexcept { return _orientation; }
        [[nodiscard]] uint32_t width() const noexcept { return _width; }
        [[nodiscard]] uint32_t height() const noexcept { return _height; }
        [[nodiscard]] uint32_t tile_width() const noexcept { return _tile_width; }
        [[nodiscard]] uint32_t tile_height() const noexcept { return _tile_height; }
        [[nodiscard]] std::string_view source_format() const noexcept { return _source_format; }

        [[nodiscard]] const std::vector<tilemap_tileset_t>& tilesets() const noexcept { return _tilesets; }
        [[nodiscard]] const std::vector<tilemap_layer_t>& layers() const noexcept { return _layers; }
        [[nodiscard]] const std::vector<tilemap_property_t>& properties() const noexcept { return _properties; }
        [[nodiscard]] const std::vector<tilemap_validation_issue_t>& validation_issues() const noexcept
        {
            return _validation_issues;
        }

        void set_orientation(const tilemap_orientation_t orientation) noexcept { _orientation = orientation; }
        void set_size(const uint32_t width, const uint32_t height) noexcept
        {
            _width = width;
            _height = height;
        }
        void set_tile_size(const uint32_t width, const uint32_t height) noexcept
        {
            _tile_width = width;
            _tile_height = height;
        }
        void set_source_format(std::string source_format) { _source_format = std::move(source_format); }

        void add_tileset(tilemap_tileset_t tileset) { _tilesets.emplace_back(std::move(tileset)); }
        void add_layer(tilemap_layer_t layer) { _layers.emplace_back(std::move(layer)); }
        void add_property(tilemap_property_t property) { _properties.emplace_back(std::move(property)); }
        void add_validation_issue(tilemap_validation_issue_t issue) { _validation_issues.emplace_back(std::move(issue)); }

    private:
        tilemap_orientation_t _orientation{ tilemap_orientation_t::orthogonal };
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        uint32_t _tile_width{ 0 };
        uint32_t _tile_height{ 0 };
        std::string _source_format;
        std::vector<tilemap_tileset_t> _tilesets;
        std::vector<tilemap_layer_t> _layers;
        std::vector<tilemap_property_t> _properties;
        std::vector<tilemap_validation_issue_t> _validation_issues;
    };

    struct tilemap_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        tilemap_asset_t tilemap;
    };
} // namespace carrot::assets
