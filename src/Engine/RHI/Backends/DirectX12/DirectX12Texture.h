//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Texture.h"

#include <memory>

namespace carrot::rhi::dx12 {
    class dx12_texture_t final : public rhi_texture_t
    {
    public:
        dx12_texture_t(const uint32_t width, const uint32_t height, const texture_format_t format,
                       ID3D12Resource* resource,
                       const DXGI_FORMAT resource_format,
                       const DXGI_FORMAT srv_format,
                       const D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
            : _width{ width }, _height{ height }, _format{ format }, _resource{ resource },
              _resource_format{ resource_format }, _srv_format{ srv_format }, _resource_state{ initial_state } {}

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
        [[nodiscard]] bool has_initial_data() const noexcept override { return _has_initial_data; }

        [[nodiscard]] ID3D12Resource* resource() const noexcept { return _resource; }
        [[nodiscard]] DXGI_FORMAT resource_format() const noexcept { return _resource_format; }
        [[nodiscard]] DXGI_FORMAT srv_format() const noexcept { return _srv_format; }
        [[nodiscard]] bool transition_to(ID3D12GraphicsCommandList* command_list,
                                         const D3D12_RESOURCE_STATES new_state) const noexcept
        {
            if (!_resource || !command_list)
                return false;

            if (_resource_state == new_state)
                return true;

            D3D12_RESOURCE_BARRIER barrier{ };
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = _resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = _resource_state;
            barrier.Transition.StateAfter = new_state;
            command_list->ResourceBarrier(1u, &barrier);

            _resource_state = new_state;
            return true;
        }
        void set_has_initial_data(const bool has_initial_data) noexcept { _has_initial_data = has_initial_data; }
        void set_resource_state(const D3D12_RESOURCE_STATES state) const noexcept { _resource_state = state; }
        [[nodiscard]] D3D12_RESOURCE_STATES resource_state() const noexcept { return _resource_state; }

    private:
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        texture_format_t _format{ texture_format_t::rgba8_srgb };
        ID3D12Resource* _resource{ nullptr };
        DXGI_FORMAT _resource_format{ DXGI_FORMAT_UNKNOWN };
        DXGI_FORMAT _srv_format{ DXGI_FORMAT_UNKNOWN };
        bool _has_initial_data{ false };
        mutable D3D12_RESOURCE_STATES _resource_state{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    };

    class dx12_render_target_t final : public rhi_render_target_t
    {
    public:
        dx12_render_target_t(std::unique_ptr<dx12_texture_t> color_texture,
                             ID3D12DescriptorHeap* rtv_heap,
                             const D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle) noexcept
            : _color_texture{ std::move(color_texture) }, _rtv_heap{ rtv_heap }, _rtv_handle{ rtv_handle } {}

        ~dx12_render_target_t() override
        {
            if (_rtv_heap)
            {
                _rtv_heap->Release();
                _rtv_heap = nullptr;
            }
        }

        [[nodiscard]] uint32_t width() const noexcept override
        {
            return _color_texture ? _color_texture->width() : 0u;
        }

        [[nodiscard]] uint32_t height() const noexcept override
        {
            return _color_texture ? _color_texture->height() : 0u;
        }

        [[nodiscard]] texture_format_t format() const noexcept override
        {
            return _color_texture ? _color_texture->format() : texture_format_t::rgba8_srgb;
        }

        [[nodiscard]] rhi_texture_t* color_texture() const noexcept override { return _color_texture.get(); }
        [[nodiscard]] dx12_texture_t* dx12_color_texture() const noexcept { return _color_texture.get(); }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle() const noexcept { return _rtv_handle; }

    private:
        std::unique_ptr<dx12_texture_t> _color_texture;
        ID3D12DescriptorHeap* _rtv_heap{ nullptr };
        D3D12_CPU_DESCRIPTOR_HANDLE _rtv_handle{ };
    };
} // namespace carrot::rhi::dx12
