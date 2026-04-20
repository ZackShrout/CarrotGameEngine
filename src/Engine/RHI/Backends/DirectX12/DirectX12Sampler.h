//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Core.h"
#include "DirectX12Common.h"
#include "RHI/Sampler.h"

namespace carrot::rhi::dx12 {
    class dx12_sampler_t final : public rhi_sampler_t
    {
    public:
        dx12_sampler_t(ID3D12Device* device, const sampler_desc_t& desc)
            : rhi_sampler_t{ desc }
        {
            if (!device)
                LOG_GRAPHICS_FATAL("dx12_sampler_t created with null device");

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            heap_desc.NumDescriptors = 1u;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            heap_desc.NodeMask = 0u;

            DX12_CHECK(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_descriptor_heap)));
            DX12_NAME(_descriptor_heap, L"DX12 Cached Sampler Heap");

            _cpu_handle = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();
            const D3D12_SAMPLER_DESC d3d_sampler_desc{ dx12_sampler_desc(desc) };
            device->CreateSampler(&d3d_sampler_desc, _cpu_handle);
        }

        ~dx12_sampler_t() override
        {
            if (_descriptor_heap)
            {
                _descriptor_heap->Release();
                _descriptor_heap = nullptr;
            }
        }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle() const noexcept { return _cpu_handle; }

        dx12_sampler_t(const dx12_sampler_t&) = delete;
        dx12_sampler_t& operator=(const dx12_sampler_t&) = delete;
        dx12_sampler_t(dx12_sampler_t&&) = delete;
        dx12_sampler_t& operator=(dx12_sampler_t&&) = delete;

    private:
        ID3D12DescriptorHeap* _descriptor_heap{ nullptr };
        D3D12_CPU_DESCRIPTOR_HANDLE _cpu_handle{ };
    };
} // namespace carrot::rhi::dx12
