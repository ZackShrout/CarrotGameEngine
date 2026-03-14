//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TextureAsset.h"

namespace carrot::assets {
    // PUBLIC

    texture_asset_t::texture_asset_t(std::shared_ptr<image_rgba8_t> image) noexcept : _image{ std::move(image) }
    {
        sync_desc_from_image();
    }

    bool texture_asset_t::valid() const noexcept
    {
        return _image != nullptr && _image->valid() && _desc.format != texture_format::unknown;
    }

    void texture_asset_t::set_image(std::shared_ptr<image_rgba8_t> image) noexcept
    {
        _image = std::move(image);
        sync_desc_from_image();
        _gpu_texture.reset();
    }

    void texture_asset_t::set_gpu_texture(std::unique_ptr<rhi::rhi_texture_t> texture) noexcept
    {
        CE_ASSERT(valid(), "Cannot set GPU texture on invalid texture asset");
        _gpu_texture = std::move(texture);
    }

    // PRIVATE

    void texture_asset_t::sync_desc_from_image() noexcept
    {
        _desc.mip_count = 1;
        _desc.format = texture_format::unknown;

        if (_image == nullptr || !_image->valid()) return;

        _desc.format = _image->is_srgb ? texture_format::rgba8_srgb : texture_format::rgba8_unorm;
    }
} // namespace carrot::assets
