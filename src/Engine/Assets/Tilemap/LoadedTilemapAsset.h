//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Texture.h"
#include "TilemapAsset.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace carrot::assets {
    class loaded_tilemap_asset_t
    {
    public:
        loaded_tilemap_asset_t() = default;
        loaded_tilemap_asset_t(tilemap_asset_t tilemap, const tilemap_asset_record_t* record) noexcept
            : _tilemap{ std::move(tilemap) }, _record{ record } {}

        [[nodiscard]] const tilemap_asset_t& tilemap() const noexcept { return _tilemap; }
        [[nodiscard]] const tilemap_asset_record_t* record() const noexcept { return _record; }
        [[nodiscard]] const std::vector<std::unique_ptr<rhi::rhi_texture_t>>& tileset_textures() const noexcept
        {
            return _tileset_textures;
        }
        [[nodiscard]] bool valid() const noexcept { return _record != nullptr; }
        [[nodiscard]] const tilemap_layer_t* find_object_layer(std::string_view name) const noexcept;
        [[nodiscard]] const tilemap_object_t* find_object_by_name(std::string_view name) const noexcept;
        [[nodiscard]] const tilemap_object_t* find_first_object_by_type(std::string_view type) const noexcept;
        [[nodiscard]] std::vector<const tilemap_object_t*> find_objects_by_type(std::string_view type) const;
        [[nodiscard]] std::vector<const tilemap_object_t*> find_objects_by_type_in_layer(std::string_view layer_name,
                                                                                          std::string_view type) const;
        [[nodiscard]] std::span<const tilemap_object_t> objects_in_layer(std::string_view name) const noexcept;

        void add_tileset_texture(std::unique_ptr<rhi::rhi_texture_t> texture)
        {
            _tileset_textures.emplace_back(std::move(texture));
        }

    private:
        tilemap_asset_t _tilemap;
        const tilemap_asset_record_t* _record{ nullptr };
        std::vector<std::unique_ptr<rhi::rhi_texture_t>> _tileset_textures;
    };
} // namespace carrot::assets
