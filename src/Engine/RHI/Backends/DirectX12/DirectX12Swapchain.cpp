//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12Swapchain.h"

#include "DirectX12Common.h"
#include "DirectX12Core.h"

namespace carrot::rhi::dx12 {
    dx12_swapchain_t::dx12_swapchain_t(ID3D12Device* device, ID3D12CommandQueue* command_queue, HWND hwnd,
                                       const uint32_t width, const uint32_t height)
        : _width{ width }, _height{ height }
    {
        IDXGIFactory6* factory{ nullptr };
        HRESULT hr{ CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)) };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DXGI factory");

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferCount = k_max_frames_in_flight;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        IDXGISwapChain1* sc1{ nullptr };
        hr = factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, nullptr, nullptr, &sc1);
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 swapchain");

        hr = sc1->QueryInterface(IID_PPV_ARGS(&_swapchain));
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to query IDXGISwapChain4");

        sc1->Release();
        factory->Release();

        D3D12_DESCRIPTOR_HEAP_DESC rtv{};
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = k_max_frames_in_flight;
        device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&_rtv_heap));

        UINT stride{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
        auto handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            hr = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i]));
            if (FAILED(hr))
                LOG_GRAPHICS_FATAL("Failed to get DX12 backbuffer {}", i);

            device->CreateRenderTargetView(_backbuffers[i], nullptr, handle);
            handle.ptr += stride;
        }

        _image_index = _swapchain->GetCurrentBackBufferIndex();
    }

    dx12_swapchain_t::~dx12_swapchain_t()
    {
        if (_swapchain)
        {
            // Be nice to drivers: ensure we are windowed before destroying.
            _swapchain->SetFullscreenState(FALSE, nullptr);
        }

        for (auto& buf : _backbuffers)
        {
            if (buf)
            {
                buf->Release();
                buf = nullptr;
            }
        }

        if (_rtv_heap)
        {
            _rtv_heap->Release();
            _rtv_heap = nullptr;
        }

        if (_swapchain)
        {
            _swapchain->Release();
            _swapchain = nullptr;
        }
    }

    void dx12_swapchain_t::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        _width = width;
        _height = height;

        // Full implementation comes later
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
        // Temporary until dx12_texture_t exists
        return reinterpret_cast<rhi_texture_t*>(_backbuffers[_image_index]);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dx12_swapchain_t::get_current_rtv(const uint32_t stride) const noexcept
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle{
            _rtv_heap->GetCPUDescriptorHandleForHeapStart()
        };

        handle.ptr += stride * _image_index;
        return handle;
    }
} // namespace carrot::rhi::dx12
