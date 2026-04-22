//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "RHI/Texture.h"

namespace carrot::rhi::metal {
    class metal_device_t;

    class metal_texture_t final : public rhi_texture_t
    {
    public:
        metal_texture_t(MTL::Texture* texture, uint32_t width, uint32_t height, texture_format_t format) noexcept;
        ~metal_texture_t() override;

        [[nodiscard]] uint32_t width() const noexcept override { return _width; }
        [[nodiscard]] uint32_t height() const noexcept override { return _height; }
        [[nodiscard]] texture_format_t format() const noexcept override { return _format; }
        [[nodiscard]] bool has_initial_data() const noexcept override { return _has_initial_data; }

        [[nodiscard]] MTL::Texture* mtl_texture() const noexcept { return _texture; }
        void set_has_initial_data(const bool has_initial_data) noexcept { _has_initial_data = has_initial_data; }

    private:
        MTL::Texture*    _texture{ nullptr };
        uint32_t         _width{ 0 };
        uint32_t         _height{ 0 };
        texture_format_t _format{ texture_format_t::rgba8_srgb };
        bool             _has_initial_data{ false };
    };

    class metal_render_target_t final : public rhi_render_target_t
    {
    public:
        metal_render_target_t() noexcept = default;
        ~metal_render_target_t() override = default;

        [[nodiscard]] uint32_t width() const noexcept override;
        [[nodiscard]] uint32_t height() const noexcept override;
        [[nodiscard]] texture_format_t format() const noexcept override;
        [[nodiscard]] rhi_texture_t* color_texture() const noexcept override { return _color_texture.get(); }

        [[nodiscard]] metal_texture_t* metal_color_texture() const noexcept { return _color_texture.get(); }
        void set_color_texture(std::unique_ptr<metal_texture_t> color_texture) noexcept
        {
            _color_texture = std::move(color_texture);
        }

    private:
        std::unique_ptr<metal_texture_t> _color_texture;
    };
} // namespace carrot::rhi::metal
