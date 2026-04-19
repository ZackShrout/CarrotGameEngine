//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12Buffer.h"

namespace carrot::rhi::dx12 {
    dx12_buffer_t::dx12_buffer_t(ID3D12Device* device, const buffer_create_info_t& info)
        : rhi_buffer_t{ info.size_bytes, info.usage }
    {
        if (!device)
            LOG_GRAPHICS_FATAL("dx12_buffer_t created with null device");

        if (info.size_bytes == 0)
            LOG_GRAPHICS_FATAL("dx12_buffer_t created with zero size");

        const bool use_upload_heap{ info.cpu_writable || buffer_usage_prefers_upload_memory(info.usage) };
        const bool use_readback_heap{ buffer_usage_prefers_readback_memory(info.usage) };
        _cpu_writable = use_upload_heap && !use_readback_heap;

        if (use_readback_heap && info.cpu_writable)
            LOG_GRAPHICS_FATAL("dx12 readback buffer cannot also request cpu_writable");

        if (use_readback_heap && info.initial_data)
            LOG_GRAPHICS_FATAL("dx12 readback buffer cannot be initialized with CPU data");

        D3D12_HEAP_PROPERTIES heap_props{ };
        heap_props.Type = use_readback_heap ? D3D12_HEAP_TYPE_READBACK
                                            : (use_upload_heap ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
        heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap_props.CreationNodeMask = 1;
        heap_props.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resource_desc{ };
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Alignment = 0;
        resource_desc.Width = info.size_bytes;
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = info.usage == buffer_usage_t::storage ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                                                    : D3D12_RESOURCE_FLAG_NONE;

        const D3D12_RESOURCE_STATES initial_state{
            use_readback_heap ? D3D12_RESOURCE_STATE_COPY_DEST
                              : (use_upload_heap ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON)
        };

        DX12_CHECK(
            device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, initial_state, nullptr,
                IID_PPV_ARGS(&_resource)));

        const wchar_t* debug_name{ L"DX12 Buffer" };

        if (use_upload_heap)
            debug_name = L"DX12 Upload Buffer";
        else if (use_readback_heap)
            debug_name = L"DX12 Readback Buffer";
        else if (info.usage == buffer_usage_t::vertex)
            debug_name = L"DX12 Vertex Buffer";
        else if (info.usage == buffer_usage_t::index)
            debug_name = L"DX12 Index Buffer";
        else if (info.usage == buffer_usage_t::uniform)
            debug_name = L"DX12 Uniform Buffer";
        else if (info.usage == buffer_usage_t::staging)
            debug_name = L"DX12 Staging Buffer";
        else if (info.usage == buffer_usage_t::storage)
            debug_name = L"DX12 Storage Buffer";
        else if (info.usage == buffer_usage_t::indirect)
            debug_name = L"DX12 Indirect Buffer";

        DX12_NAME(_resource, debug_name);

        if (use_upload_heap || use_readback_heap)
        {
            DX12_CHECK(_resource->Map(0, nullptr, &_mapped_ptr));
        }

        if (info.initial_data)
        {
            if (!write(info.initial_data, info.size_bytes, 0))
                LOG_GRAPHICS_FATAL("Failed to initialize DX12 buffer contents");
        }
    }

    dx12_buffer_t::~dx12_buffer_t()
    {
        if (_resource)
        {
            if (_mapped_ptr)
            {
                _resource->Unmap(0, nullptr);
                _mapped_ptr = nullptr;
            }

            _resource->Release();
            _resource = nullptr;
        }
    }

    bool dx12_buffer_t::write(const void* data, size_t size_bytes, size_t offset_bytes/* = 0*/)
    {
        if (!data)
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::write called with null data");
            return false;
        }

        if (!_cpu_writable || !_mapped_ptr)
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::write called on non-CPU-writable {} buffer",
                               buffer_usage_to_string(usage()));
            return false;
        }

        if (offset_bytes + size_bytes > this->size_bytes())
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::write out of bounds (offset={}, size={}, capacity={})",
                               offset_bytes, size_bytes, this->size_bytes());
            return false;
        }

        std::memcpy(static_cast<std::byte*>(_mapped_ptr) + offset_bytes, data, size_bytes);

        return true;
    }
} // namespace carrot::rhi::dx12
