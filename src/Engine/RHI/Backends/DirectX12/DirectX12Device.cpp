//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12Device.h"

#include "RHI/RHI.h"

namespace carrot::rhi::dx12 {
    dx12_device_t::dx12_device_t(const rhi_desc_t& desc)
    {
#if defined(_DEBUG)
        if (desc.enable_debug_layers)
        {
            ID3D12Debug* debug{ nullptr };
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                debug->EnableDebugLayer();
                debug->Release();
            }
            else
            {
                LOG_GRAPHICS_WARN("Failed to enable DX12 debug layer");
            }
        }
#endif

        IDXGIFactory6* factory{ nullptr };
        DX12_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));

        IDXGIAdapter1* adapter{ nullptr };

        for (uint32_t i{ 0 }; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 adapter_desc{ };
            DX12_CHECK(adapter->GetDesc1(&adapter_desc));

            if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                adapter->Release();
                adapter = nullptr;
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&_device))))
            {
                adapter->Release();
                adapter = nullptr;
                break;
            }

            adapter->Release();
            adapter = nullptr;
        }

        if (!_device)
            LOG_GRAPHICS_FATAL("Failed to create D3D12 device");

        DX12_NAME(_device, L"DX12 Device");

        if (factory)
        {
            factory->Release();
            factory = nullptr;
        }
    }

    dx12_device_t::~dx12_device_t()
    {
        if (_device)
        {
#if defined(_DEBUG)
            ID3D12DebugDevice* debug{ nullptr };
            if (SUCCEEDED(_device->QueryInterface(IID_PPV_ARGS(&debug))))
            {
                DX12_CHECK(debug->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL));
                debug->Release();
            }
#endif

            _device->Release();
            _device = nullptr;
        }
    }

} // namespace carrot::rhi::dx12
