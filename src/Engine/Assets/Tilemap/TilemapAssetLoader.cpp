//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetLoader.h"

#include "Assets/Image/ImageAssetImporter.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::filesystem::path normalize_tileset_image_path(std::string_view raw_path)
        {
            std::string normalized{ raw_path };
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            return std::filesystem::path{ normalized };
        }
    }

    tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record)
    {
        if (record.logical_id.empty())
        {
            return {
                .asset = { },
                .error = tilemap_asset_load_error_t::invalid_record
            };
        }

        return {
            .asset = loaded_tilemap_asset_t{ record.tilemap, &record },
            .error = tilemap_asset_load_error_t::none
        };
    }

    tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record,
                                                   const io::virtual_file_system_t& vfs,
                                                   rhi::rhi_context_t& rhi) noexcept
    {
        tilemap_asset_load_result_t result{ load_tilemap_asset(record) };
        if (!result.success())
            return result;

        const auto source_path{ vfs.resolve_native_path(record.source_uri) };
        if (!source_path)
        {
            result.error = tilemap_asset_load_error_t::resolve_failed;
            return result;
        }

        for (const tilemap_tileset_t& tileset : record.tilemap.tilesets())
        {
            if (tileset.image_source_uri.empty())
            {
                result.asset.add_tileset_texture(nullptr);
                continue;
            }

            std::filesystem::path image_path{ normalize_tileset_image_path(tileset.image_source_uri) };
            if (!image_path.is_absolute())
                image_path = source_path->parent_path() / image_path;

            image_path = image_path.lexically_normal();

            if (!std::filesystem::exists(image_path))
            {
                LOG_ASSET_ERROR("Tilemap tileset image not found: raw='{}', resolved='{}'",
                                tileset.image_source_uri, image_path.string());
                result.error = tilemap_asset_load_error_t::source_not_found;
                return result;
            }

            image_load_result_t image_result{ load_image_rgba8(image_path) };
            if (!image_result.success())
            {
                result.error = tilemap_asset_load_error_t::decode_failed;
                return result;
            }

            rhi::texture_create_info_t texture_info{ };
            texture_info.width = image_result.image.width;
            texture_info.height = image_result.image.height;
            texture_info.format = image_result.image.is_srgb ? rhi::texture_format_t::rgba8_srgb
                                                             : rhi::texture_format_t::rgba8_unorm;
            texture_info.initial_data = image_result.image.data();
            texture_info.initial_data_size = image_result.image.size_bytes();
            texture_info.initial_data_stride_bytes = image_result.image.stride_bytes;

            std::unique_ptr<rhi::rhi_texture_t> texture{ rhi.create_texture_2d(texture_info) };
            if (!texture)
            {
                result.error = tilemap_asset_load_error_t::texture_create_failed;
                return result;
            }

            result.asset.add_tileset_texture(std::move(texture));
        }

        return result;
    }
} // namespace carrot::assets
