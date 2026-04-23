//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12UploadRing.h"

namespace carrot::rhi::dx12 {
    dx12_upload_ring_t::dx12_upload_ring_t(ID3D12Device* const device, const size_t capacity_bytes)
        : _allocator{ capacity_bytes }
    {
        if (!device)
            LOG_GRAPHICS_FATAL("DX12 upload ring created with null device");

        if (capacity_bytes == 0u)
            LOG_GRAPHICS_FATAL("DX12 upload ring created with zero capacity");

        D3D12_HEAP_PROPERTIES upload_heap{ };
        upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        upload_heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        upload_heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        upload_heap.CreationNodeMask = 1u;
        upload_heap.VisibleNodeMask = 1u;

        D3D12_RESOURCE_DESC upload_desc{ };
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Alignment = 0u;
        upload_desc.Width = capacity_bytes;
        upload_desc.Height = 1u;
        upload_desc.DepthOrArraySize = 1u;
        upload_desc.MipLevels = 1u;
        upload_desc.Format = DXGI_FORMAT_UNKNOWN;
        upload_desc.SampleDesc.Count = 1u;
        upload_desc.SampleDesc.Quality = 0u;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        upload_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        DX12_CHECK(device->CreateCommittedResource(&upload_heap,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &upload_desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr,
                                                   IID_PPV_ARGS(&_resource)));
        DX12_NAME(_resource, L"DX12 Upload Ring");

        void* mapped{ nullptr };
        DX12_CHECK(_resource->Map(0u, nullptr, &mapped));
        _mapped_ptr = static_cast<std::byte*>(mapped);
    }

    dx12_upload_ring_t::~dx12_upload_ring_t()
    {
        if (_resource)
        {
            _resource->Unmap(0u, nullptr);
            _resource->Release();
            _resource = nullptr;
        }
        _mapped_ptr = nullptr;
    }

    std::optional<dx12_upload_ring_t::allocation_t> dx12_upload_ring_t::allocate(const size_t size_bytes,
                                                                                  const size_t alignment) noexcept
    {
        const auto alloc{ _allocator.allocate(size_bytes, alignment) };
        if (!alloc || !_resource || !_mapped_ptr)
            return std::nullopt;

        return allocation_t{
            .resource = _resource,
            .mapped_ptr = _mapped_ptr + alloc->offset_bytes,
            .offset_bytes = alloc->offset_bytes,
            .size_bytes = alloc->size_bytes,
            .wrapped = alloc->wrapped
        };
    }

    void dx12_upload_ring_t::reset() noexcept
    {
        _allocator.reset();
    }
} // namespace carrot::rhi::dx12
