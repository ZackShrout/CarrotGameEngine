//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12RHIContext.h"

#include "DirectX12CommandList.h"
#include "DirectX12CommandQueue.h"
#include "DirectX12Device.h"
#include "DirectX12Fence.h"
#include "DirectX12Swapchain.h"
#include "RHI/RHI.h"
#include "Window/Window.h"

namespace carrot::rhi::dx12 {
    dx12_rhi_context_t::dx12_rhi_context_t(const rhi_desc_t& desc)
    {
        if (core::platform::current_platform() != core::platform::platform_type::win32)
            LOG_GRAPHICS_FATAL("DX12 backend requires Win32 platform");

        core::platform::native_window_handle_t window{ window::get_native_handle() };

        HWND hwnd{ static_cast<HWND>(window.win32_t.hwnd) };
        if (!hwnd)
            LOG_GRAPHICS_FATAL("Invalid HWND passed to DX12 context");

        _device = std::make_unique<dx12_device_t>(desc);
        _graphics_queue = std::make_unique<dx12_command_queue_t>(_device->id3d12_device());
        _swapchain = std::make_unique<dx12_swapchain_t>(_device->id3d12_device(),
                                                        _graphics_queue->id3d12_command_queue(), hwnd, desc.width,
                                                        desc.height);

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            HRESULT hr{
                _device->id3d12_device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                 IID_PPV_ARGS(&frame.allocator))
            };

            if (FAILED(hr))
                LOG_GRAPHICS_FATAL("Failed to create DX12 command allocator");

            frame.command_list = std::make_unique<dx12_command_list_t>(_device->id3d12_device(), frame.allocator);
            frame.fence = std::make_unique<dx12_fence_t>(_device->id3d12_device());
            frame.fence_value = 0;
        }

        _rtv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    dx12_rhi_context_t::~dx12_rhi_context_t()
    {
        wait_idle();

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            frame.command_list.reset();
            frame.fence.reset();

            if (frame.allocator)
            {
                frame.allocator->Release();
                frame.allocator = nullptr;
            }
        }

        _swapchain.reset();
        _graphics_queue.reset();
        _device.reset();
    }

    void dx12_rhi_context_t::begin_frame()
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        frame.fence->wait(frame.fence_value);

        frame.allocator->Reset();
        frame.command_list->set_allocator(frame.allocator);
        frame.command_list->reset();
        frame.command_list->begin_recording();

        _swapchain->acquire_next_image(nullptr);
    }

    void dx12_rhi_context_t::record_frame()
    {
        ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
        dx12_swapchain_t* sc{ _swapchain.get() };

        UINT stride{ _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };

        float clear[]{ 0.02f, 0.02f, 0.04f, 1.0f };

        D3D12_CPU_DESCRIPTOR_HANDLE rtv{ sc->get_current_rtv(stride) };

        ID3D12Resource* backbuffer{ sc->get_backbuffer(sc->get_current_image_index()) };

        D3D12_RESOURCE_BARRIER to_rtv{};
        to_rtv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_rtv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_rtv.Transition.pResource = backbuffer;
        to_rtv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_rtv.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        to_rtv.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

        cmd->ResourceBarrier(1, &to_rtv);

        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd->ClearRenderTargetView(sc->get_current_rtv(stride), clear, 0, nullptr);

        std::swap(to_rtv.Transition.StateBefore, to_rtv.Transition.StateAfter);

        cmd->ResourceBarrier(1, &to_rtv);
    }

    void dx12_rhi_context_t::end_frame()
    {
        auto& f{ _frames[_frame_index] };
        f.command_list->end_recording();

        _graphics_queue->submit(f.command_list.get(), f.fence.get(), nullptr, nullptr);
        f.fence_value = f.fence->current_value();
        _swapchain->present(nullptr);

        _frame_index = (_frame_index + 1) % k_max_frames_in_flight;
    }

    void dx12_rhi_context_t::resize(uint32_t width, uint32_t height)
    {

    }

    rhi_device_t* dx12_rhi_context_t::get_device() const noexcept
    {
        return _device.get();
    }

    rhi_swapchain_t* dx12_rhi_context_t::get_swapchain() const noexcept
    {
        return _swapchain.get();
    }

    rhi_command_queue_t* dx12_rhi_context_t::get_command_queue() const noexcept
    {
        return _graphics_queue.get();
    }

    void dx12_rhi_context_t::wait_idle()
    {
        if (_graphics_queue)
            _graphics_queue->wait_idle();

        // These should all be completed now, but we are being safe
        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
             _frames[i].fence->wait(_frames[i].fence_value);
    }
} // namespace carrot::rhi::dx12
