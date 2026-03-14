//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Image/ImageAsset.h"
#include "RHI/Texture.h"

#include <memory>

namespace carrot::rhi {
    class rhi_texture_t;
}

namespace carrot::assets {
    enum class texture_format : uint8_t
    {
        unknown = 0,
        rgba8_unorm,
        rgba8_srgb,
    };

    struct texture_desc_t
    {
        uint32_t mip_count{ 1 };
        texture_format format{ texture_format::unknown };
    };

    class texture_asset_t
    {
    public:
        texture_asset_t() = default;
        explicit texture_asset_t(std::shared_ptr<image_rgba8_t> image) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] bool has_image() const noexcept { return _image != nullptr; }
        [[nodiscard]] bool has_gpu_texture() const noexcept { return _gpu_texture != nullptr; }

        [[nodiscard]] const image_rgba8_t* image() const noexcept { return _image.get(); }
        [[nodiscard]] image_rgba8_t* image() noexcept { return _image.get(); }

        [[nodiscard]] uint32_t width() const noexcept { return _image == nullptr ? 0 : _image->width; }
        [[nodiscard]] uint32_t height() const noexcept { return _image == nullptr ? 0 : _image->height; }
        [[nodiscard]] uint32_t stride_bytes() const noexcept { return _image == nullptr ? 0 : _image->stride_bytes; }

        [[nodiscard]] const uint8_t* data() const noexcept { return _image == nullptr ? nullptr : _image->data(); }
        [[nodiscard]] uint8_t* data() noexcept { return _image == nullptr ? nullptr : _image->data(); }
        [[nodiscard]] size_t size_bytes() const noexcept { return _image == nullptr ? 0 : _image->size_bytes(); }

        [[nodiscard]] const texture_desc_t& desc() const noexcept { return _desc; }
        [[nodiscard]] texture_format format() const noexcept { return _desc.format; }
        [[nodiscard]] bool is_srgb() const noexcept { return _desc.format == texture_format::rgba8_srgb; }

        [[nodiscard]] const rhi::rhi_texture_t* gpu_texture() const noexcept { return _gpu_texture.get(); }
        [[nodiscard]] rhi::rhi_texture_t* gpu_texture() noexcept { return _gpu_texture.get(); }

        void set_image(std::shared_ptr<image_rgba8_t> image) noexcept;
        void set_gpu_texture(std::unique_ptr<rhi::rhi_texture_t> texture) noexcept;

    private:
        void sync_desc_from_image() noexcept;

        std::shared_ptr<image_rgba8_t> _image{};
        texture_desc_t _desc{};
        std::unique_ptr<rhi::rhi_texture_t> _gpu_texture{};
    };
} // namespace carrot::assets
