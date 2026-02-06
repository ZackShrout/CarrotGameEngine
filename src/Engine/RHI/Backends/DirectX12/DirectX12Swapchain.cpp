//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12Swapchain.h"

#include "DirectX12Common.h"
#include "DirectX12Core.h"
#include "Window/Window.h"

namespace carrot::rhi::dx12 {
    dx12_swapchain_t::dx12_swapchain_t(ID3D12Device* device, ID3D12CommandQueue* command_queue, const uint32_t width, const uint32_t height)
        : _width{ width }, _height{ height }
    {
        core::platform::native_window_handle_t window_handle{ window::get_native_handle() };
        IDXGIFactory6* factory{ nullptr };

        HRESULT hr{ CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)) };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DXGI factory");

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = _width;
        desc.Height = _height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = k_max_frames_in_flight;
        desc.SampleDesc.Count = 1;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Scaling = DXGI_SCALING_STRETCH;

        IDXGISwapChain1* swapchain1{ nullptr };
        hr = factory->CreateSwapChainForHwnd(
            command_queue,
            static_cast<HWND>(window_handle.win32_t.hwnd),
            &desc,
            nullptr,
            nullptr,
            &swapchain1);

        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 swapchain");

        hr = swapchain1->QueryInterface(IID_PPV_ARGS(&_swapchain));
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to query IDXGISwapChain4");

        swapchain1->Release();
        factory->Release();

        _image_index = _swapchain->GetCurrentBackBufferIndex();
        _image_count = k_max_frames_in_flight;

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.NumDescriptors = k_max_frames_in_flight;


        hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap));
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create RTV heap");


        uint32_t rtv_stride{ device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };

        D3D12_CPU_DESCRIPTOR_HANDLE handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            ID3D12Resource* backbuffer{ nullptr };
            hr = _swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
            if (FAILED(hr))
                LOG_GRAPHICS_FATAL("Failed to get DX12 backbuffer {}", i);

            device->CreateRenderTargetView(backbuffer, nullptr, handle);

            handle.ptr += rtv_stride;
            backbuffer->Release();
        }
    }

    dx12_swapchain_t::~dx12_swapchain_t()
    {
        if (_rtv_heap) _rtv_heap->Release();
        if (_swapchain) _swapchain->Release();
    }

    void dx12_swapchain_t::resize(uint32_t width, uint32_t height)
    {

    }

    uint32_t dx12_swapchain_t::acquire_next_image([[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        _image_index = _swapchain->GetCurrentBackBufferIndex();
        return _image_index;
    }

    void dx12_swapchain_t::present([[maybe_unused]] rhi_semaphore_t* wait_semaphore)
    {
        _swapchain->Present(1, 0);
    }

    rhi_texture_t* dx12_swapchain_t::get_current_backbuffer() const
    {
        return nullptr;
    }
} // namespace carrot::rhi::dx12
