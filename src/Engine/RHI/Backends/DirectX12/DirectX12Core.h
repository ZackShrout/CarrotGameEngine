//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Sampler.h"
#include "RHI/Texture.h"

namespace carrot::rhi::dx12 {
    constexpr uint32_t k_max_frames_in_flight{ 3 };

    [[nodiscard]] inline DXGI_FORMAT dx12_backbuffer_format() noexcept
    {
        // Swapchain resource format.
        // We keep the underlying backbuffer resource as UNORM and create an sRGB RTV
        // so writes match Vulkan/Metal color behavior.
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    [[nodiscard]] inline DXGI_FORMAT dx12_backbuffer_rtv_format() noexcept
    {
        // Render-target view format used when drawing into the swapchain.
        // This enables hardware sRGB write conversion for parity with Vulkan/Metal.
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    [[nodiscard]] inline DXGI_FORMAT dx12_texture_resource_format(const texture_format_t format) noexcept
    {
        switch (format)
        {
            case texture_format_t::rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case texture_format_t::rgba8_srgb:  return DXGI_FORMAT_R8G8B8A8_UNORM;
            default:                            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    [[nodiscard]] inline DXGI_FORMAT dx12_texture_srv_format(const texture_format_t format) noexcept
    {
        switch (format)
        {
            case texture_format_t::rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case texture_format_t::rgba8_srgb:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            default:                            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    [[nodiscard]] inline D3D12_FILTER dx12_filter(const sampler_desc_t& desc) noexcept
    {
        const bool min_linear{ desc.min_filter == sampler_filter_t::linear };
        const bool mag_linear{ desc.mag_filter == sampler_filter_t::linear };
        const bool mip_linear{ desc.mip_filter == sampler_mip_filter_t::linear };

        if (!min_linear && !mag_linear && !mip_linear) return D3D12_FILTER_MIN_MAG_MIP_POINT;
        if (!min_linear && !mag_linear &&  mip_linear) return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        if (!min_linear &&  mag_linear && !mip_linear) return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        if (!min_linear &&  mag_linear &&  mip_linear) return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        if ( min_linear && !mag_linear && !mip_linear) return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        if ( min_linear && !mag_linear &&  mip_linear) return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        if ( min_linear &&  mag_linear && !mip_linear) return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }

    [[nodiscard]] inline D3D12_TEXTURE_ADDRESS_MODE dx12_address_mode(const sampler_address_mode_t mode) noexcept
    {
        switch (mode)
        {
            case sampler_address_mode_t::clamp_to_edge:   return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case sampler_address_mode_t::repeat:          return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            case sampler_address_mode_t::mirrored_repeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            default:                                      return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        }
    }

    [[nodiscard]] inline D3D12_SAMPLER_DESC dx12_sampler_desc(const sampler_desc_t& desc) noexcept
    {
        D3D12_SAMPLER_DESC result{ };
        result.Filter = dx12_filter(desc);
        result.AddressU = dx12_address_mode(desc.address_u);
        result.AddressV = dx12_address_mode(desc.address_v);
        result.AddressW = dx12_address_mode(desc.address_w);
        result.MipLODBias = desc.mip_lod_bias;
        result.MinLOD = desc.min_lod;
        result.MaxLOD = desc.max_lod;
        result.MaxAnisotropy = 1;
        result.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        result.BorderColor[0] = 0.f;
        result.BorderColor[1] = 0.f;
        result.BorderColor[2] = 0.f;
        result.BorderColor[3] = 0.f;
        return result;
    }
} // namespace carrot::rhi::dx12
