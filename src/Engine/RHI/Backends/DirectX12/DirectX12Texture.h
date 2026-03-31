//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Texture.h"

namespace carrot::rhi::dx12 {
    class dx12_texture_t final : public rhi_texture_t
    {
    public:
        dx12_texture_t(const uint32_t width, const uint32_t height, const texture_format_t format,
                       ID3D12Resource* resource, const DXGI_FORMAT resource_format, const DXGI_FORMAT srv_format)
            : _width{ width }, _height{ height }, _format{ format }, _resource{ resource },
              _resource_format{ resource_format }, _srv_format{ srv_format } {}

        ~dx12_texture_t() override
        {
            if (_resource)
            {
                _resource->Release();
                _resource = nullptr;
            }
        }

        [[nodiscard]] uint32_t width() const noexcept override { return _width; }
        [[nodiscard]] uint32_t height() const noexcept override { return _height; }
        [[nodiscard]] texture_format_t format() const noexcept override { return _format; }

        [[nodiscard]] ID3D12Resource* resource() const noexcept { return _resource; }
        [[nodiscard]] DXGI_FORMAT resource_format() const noexcept { return _resource_format; }
        [[nodiscard]] DXGI_FORMAT srv_format() const noexcept { return _srv_format; }

    private:
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        texture_format_t _format{ texture_format_t::rgba8_srgb };
        ID3D12Resource* _resource{ nullptr };
        DXGI_FORMAT _resource_format{ DXGI_FORMAT_UNKNOWN };
        DXGI_FORMAT _srv_format{ DXGI_FORMAT_UNKNOWN };
    };
} // namespace carrot::rhi::dx12
