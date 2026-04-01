//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"

#include <cstdint>
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

    struct tilemap_property_t
    {
        std::string name;
        tilemap_property_value_t value;
    };

    struct tilemap_tileset_t
    {
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
    };

    struct tilemap_object_t
    {
        uint32_t id{ 0 };
        std::string name;
        std::string type;
        float x{ 0.f };
        float y{ 0.f };
        float width{ 0.f };
        float height{ 0.f };
        float rotation{ 0.f };
        bool visible{ true };
        uint32_t gid{ 0 };
        std::vector<tilemap_property_t> properties;
    };

    struct tilemap_layer_t
    {
        tilemap_layer_kind_t kind{ tilemap_layer_kind_t::tile };
        std::string name;
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        bool visible{ true };
        float opacity{ 1.f };

        // Tile layers populate gids. Object layers populate objects.
        std::vector<uint32_t> gids;
        std::vector<tilemap_object_t> objects;
        std::vector<tilemap_property_t> properties;
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
    };

    struct tilemap_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        tilemap_asset_t tilemap;
    };
} // namespace carrot::assets
