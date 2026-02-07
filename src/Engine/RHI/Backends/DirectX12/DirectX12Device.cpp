//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12Device.h"

#include "RHI/RHI.h"

namespace carrot::rhi::dx12 {
    dx12_device_t::dx12_device_t(const rhi_desc_t& desc)
    {
#if defined(_DEBUG)
        // if (desc.enable_debug_layers)
        // {
        //     ID3D12Debug* debug{ nullptr };
        //     if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        //     {
        //         debug->EnableDebugLayer();
        //         debug->Release();
        //     }
        // }
#endif

        IDXGIFactory6* factory{ nullptr };
        HRESULT hr{ CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)) };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create IDXGI Factory");

        IDXGIAdapter1* adapter{ nullptr };

        for (uint32_t i{ 0 };
             factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                adapter->Release();
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(
                adapter,
                D3D_FEATURE_LEVEL_12_0,
                IID_PPV_ARGS(&_device))))
            {
                break;
            }

            adapter->Release();
            adapter = nullptr;
        }

        if (!_device)
            LOG_GRAPHICS_FATAL("Failed to create D3D12 device");

        factory->Release();
    }

    dx12_device_t::~dx12_device_t()
    {
        if (_device)
        {
#if defined(_DEBUG)
            ID3D12DebugDevice* debug{ nullptr };
            if (SUCCEEDED(_device->QueryInterface(IID_PPV_ARGS(&debug))))
            {
                debug->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
                debug->Release();
            }
#endif

            _device->Release();
            _device = nullptr;
        }
    }

    rhi_command_queue_t* dx12_device_t::create_command_queue(queue_type type)
    {
        return nullptr;
    }

    rhi_swapchain_t* dx12_device_t::create_swapchain(uint32_t width, uint32_t height)
    {
        return nullptr;
    }

    rhi_buffer_t* dx12_device_t::create_buffer(const buffer_desc_t& desc)
    {
        return nullptr;
    }

    rhi_texture_t* dx12_device_t::create_texture()
    {
        return nullptr;
    }

    rhi_graphics_pipeline_t* dx12_device_t::create_graphics_pipeline()
    {
        return nullptr;
    }

    void dx12_device_t::destroy_buffer(rhi_buffer_t* buffer)
    {

    }
} // namespace carrot::rhi::dx12
