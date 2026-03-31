//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12Swapchain.h"

#include "DirectX12Common.h"
#include "DirectX12Core.h"

namespace carrot::rhi::dx12 {
    dx12_swapchain_t::dx12_swapchain_t(ID3D12Device* device, ID3D12CommandQueue* command_queue, HWND hwnd,
                                       const uint32_t width, const uint32_t height)
        : _device{ device }, _width{ width }, _height{ height }
    {
        IDXGIFactory6* factory{ nullptr };
        DX12_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));

        DXGI_SWAP_CHAIN_DESC1 desc{ };
        desc.Width = width;
        desc.Height = height;
        desc.Format = dx12_backbuffer_format();
        desc.BufferCount = k_max_frames_in_flight;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        IDXGISwapChain1* sc1{ nullptr };
        DX12_CHECK(factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, nullptr, nullptr, &sc1));
        DX12_CHECK(sc1->QueryInterface(IID_PPV_ARGS(&_swapchain)));

        IDXGIFactory* parent_factory{ nullptr };
        if (SUCCEEDED(_swapchain->GetParent(IID_PPV_ARGS(&parent_factory))) && parent_factory)
        {
            if (FAILED(parent_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES)))
                LOG_GRAPHICS_DEBUG("DX12: MakeWindowAssociation failed; falling back to default Alt+Enter behavior");

            parent_factory->Release();
        }

        sc1->Release();
        factory->Release();

        D3D12_DESCRIPTOR_HEAP_DESC rtv{ };
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = k_max_frames_in_flight;
        rtv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtv.NodeMask = 0;

        DX12_CHECK(device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&_rtv_heap)));
        DX12_NAME(_rtv_heap, L"DX12 Swapchain RTV Heap");

        UINT stride{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
        D3D12_CPU_DESCRIPTOR_HANDLE handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };

        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{ };
        rtv_desc.Format = dx12_backbuffer_rtv_format();
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;
        rtv_desc.Texture2D.PlaneSlice = 0;

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            DX12_CHECK(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i])));

            device->CreateRenderTargetView(_backbuffers[i], &rtv_desc, handle);
            DX12_NAME_INDEXED(_backbuffers[i], i, L"DX12 Swapchain Backbuffer");

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

        for (auto& buf: _backbuffers)
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

        if (!_swapchain || !_device)
            return;

        if (width == _width && height == _height)
            return;

        _width = width;
        _height = height;

        // Release old backbuffers
        for (auto& buf: _backbuffers)
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

        // Get current swapchain description so we preserve format/flags
        DXGI_SWAP_CHAIN_DESC desc{ };
        DX12_CHECK(_swapchain->GetDesc(&desc));

        // Resize buffers
        const HRESULT hr{
            _swapchain->ResizeBuffers(k_max_frames_in_flight, width, height, desc.BufferDesc.Format, desc.Flags)
        };

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            LOG_GRAPHICS_FATAL("DX12 device lost during ResizeBuffers (hr={:#x})", static_cast<uint32_t>(hr));

        DX12_CHECK(hr);

        // Recreate RTV heap
        D3D12_DESCRIPTOR_HEAP_DESC rtv{ };
        rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv.NumDescriptors = k_max_frames_in_flight;
        rtv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtv.NodeMask = 0;

        DX12_CHECK(_device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&_rtv_heap)));
        DX12_NAME(_rtv_heap, L"DX12 Swapchain RTV Heap");

        const UINT stride{ _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
        D3D12_CPU_DESCRIPTOR_HANDLE handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };

        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{ };
        rtv_desc.Format = dx12_backbuffer_rtv_format();
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;
        rtv_desc.Texture2D.PlaneSlice = 0;

        for (uint32_t i = 0; i < k_max_frames_in_flight; ++i)
        {
            DX12_CHECK(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i])));

            _device->CreateRenderTargetView(_backbuffers[i], &rtv_desc, handle);
            DX12_NAME_INDEXED(_backbuffers[i], i, L"DX12 Swapchain Backbuffer");

            handle.ptr += stride;
        }

        _image_index = _swapchain->GetCurrentBackBufferIndex();
    }

    uint32_t dx12_swapchain_t::acquire_next_image([[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        _image_index = _swapchain->GetCurrentBackBufferIndex();
        return _image_index;
    }

    void dx12_swapchain_t::present([[maybe_unused]] rhi_semaphore_t* wait_semaphore)
    {
        DX12_CHECK(_swapchain->Present(1, 0));
    }

    rhi_texture_t* dx12_swapchain_t::get_current_backbuffer() const
    {
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
