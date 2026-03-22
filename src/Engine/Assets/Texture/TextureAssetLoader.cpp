//
// Created by Zack Shrout on 3/21/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetLoader.h"

#include "Assets/Image/ImageAssetImporter.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"

namespace carrot::assets {
    texture_asset_load_result_t load_texture_asset(const texture_asset_record_t& record,
                                                   const io::virtual_file_system_t& vfs,
                                                   rhi::rhi_context_t& rhi) noexcept
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
            return { { }, texture_asset_load_error::invalid_record };

        const auto native_path{ vfs.resolve_native_path(record.source_uri) };
        if (!native_path)
            return { { }, texture_asset_load_error::resolve_failed };

        if (!std::filesystem::exists(*native_path))
            return { { }, texture_asset_load_error::source_not_found };

        image_load_result_t image_result{ load_image_rgba8(*native_path) };
        if (!image_result.success())
            return { { }, texture_asset_load_error::decode_failed };

        rhi::texture_create_info_t texture_info{ };
        texture_info.width = image_result.image.width;
        texture_info.height = image_result.image.height;
        texture_info.format = record.srgb ? rhi::texture_format_t::rgba8_srgb : rhi::texture_format_t::rgba8_unorm;
        texture_info.initial_data = image_result.image.data();
        texture_info.initial_data_size = image_result.image.size_bytes();
        texture_info.initial_data_stride_bytes = image_result.image.stride_bytes;

        std::unique_ptr<rhi::rhi_texture_t> texture{ rhi.create_texture_2d(texture_info) };
        if (!texture)
            return { { }, texture_asset_load_error::texture_create_failed };

        loaded_texture_asset_t loaded{ };
        loaded.record = &record;
        loaded.texture = std::move(texture);

        return { std::move(loaded), texture_asset_load_error::ok };
    }
} // namespace carrot::assets
