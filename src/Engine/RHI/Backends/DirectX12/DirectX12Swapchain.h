//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <array>

#include "DirectX12Common.h"
#include "DirectX12Core.h"
#include "RHI/Swapchain.h"

namespace carrot::rhi::dx12 {
    class dx12_swapchain_t final : public rhi_swapchain_t
    {
    public:
        dx12_swapchain_t(ID3D12Device* device, ID3D12CommandQueue* command_queue, HWND hwnd, uint32_t width,
                         uint32_t height);
        ~dx12_swapchain_t() override;

        void resize(uint32_t width, uint32_t height) override;

        uint32_t acquire_next_image(rhi_semaphore_t* signal_semaphore) override;
        void present(rhi_semaphore_t* wait_semaphore) override;

        [[nodiscard]] rhi_texture_t* get_current_backbuffer() const override;
        [[nodiscard]] uint32_t get_current_image_index() const override { return _image_index; }
        [[nodiscard]] uint32_t get_image_count() const override { return _image_count; }
        [[nodiscard]] uint32_t get_width() const override { return _width; }
        [[nodiscard]] uint32_t get_height() const override { return _height; }

        // Accessors for internal use
        [[nodiscard]] IDXGISwapChain4* idxgi_swap_chain() const noexcept { return _swapchain; }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_current_rtv(uint32_t stride) const noexcept;

        // Temporary until we implement dx12_texture_t
        [[nodiscard]] ID3D12Resource* get_backbuffer(const uint32_t index) const noexcept { return _backbuffers[index]; }

    private:
        ID3D12Device*                                       _device{ nullptr };
        IDXGISwapChain4*                                    _swapchain{ nullptr };
        ID3D12DescriptorHeap*                               _rtv_heap{ nullptr };
        std::array<ID3D12Resource*, k_max_frames_in_flight> _backbuffers{};
        uint32_t                                            _width{ 0 };
        uint32_t                                            _height{ 0 };
        uint32_t                                            _image_index{ 0 };
        uint32_t                                            _image_count{ k_max_frames_in_flight };
    };
} // namespace carrot::rhi::dx12
