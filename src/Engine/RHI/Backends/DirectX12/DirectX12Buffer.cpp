//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12Buffer.h"

namespace carrot::rhi::dx12 {
    namespace {
        constexpr std::size_t k_shader_read_upload_shadow_threshold_bytes{ 1u };
    }

    dx12_buffer_t::dx12_buffer_t(ID3D12Device* device, const buffer_create_info_t& info)
        : rhi_buffer_t{ info.size_bytes, info.usage }
    {
        if (!device)
            LOG_GRAPHICS_FATAL("dx12_buffer_t created with null device");

        if (info.size_bytes == 0)
            LOG_GRAPHICS_FATAL("dx12_buffer_t created with zero size");

        const bool use_upload_heap{ info.cpu_writable || buffer_usage_prefers_upload_memory(info.usage) };
        const bool use_readback_heap{ buffer_usage_prefers_readback_memory(info.usage) };
        const bool use_upload_shadow_for_shader_read{
            info.usage == buffer_usage_t::shader_read &&
            info.cpu_writable &&
            !use_readback_heap &&
            info.size_bytes >= k_shader_read_upload_shadow_threshold_bytes
        };
        const bool use_upload_shadow_for_storage{
            info.usage == buffer_usage_t::storage && info.cpu_writable && !use_readback_heap
        };
        const bool use_upload_shadow{ use_upload_shadow_for_storage || use_upload_shadow_for_shader_read };
        const bool use_direct_upload_heap{ use_upload_heap && !use_upload_shadow };
        _cpu_writable = (use_direct_upload_heap || use_upload_shadow) && !use_readback_heap;

        if (use_readback_heap && info.cpu_writable)
            LOG_GRAPHICS_FATAL("dx12 readback buffer cannot also request cpu_writable");

        if (use_readback_heap && info.initial_data)
            LOG_GRAPHICS_FATAL("dx12 readback buffer cannot be initialized with CPU data");

        D3D12_HEAP_PROPERTIES heap_props{ };
        heap_props.Type = use_readback_heap ? D3D12_HEAP_TYPE_READBACK
                                            : (use_direct_upload_heap ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
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
        resource_desc.Flags =
            (info.usage == buffer_usage_t::storage || info.usage == buffer_usage_t::indirect)
                ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                : D3D12_RESOURCE_FLAG_NONE;

        const D3D12_RESOURCE_STATES initial_state{
            use_readback_heap ? D3D12_RESOURCE_STATE_COPY_DEST
                              : (use_direct_upload_heap ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON)
        };
        _resource_state = initial_state;

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
        else if (info.usage == buffer_usage_t::shader_read)
            debug_name = L"DX12 Shader-Read Buffer";
        else if (info.usage == buffer_usage_t::storage)
            debug_name = L"DX12 Storage Buffer";
        else if (info.usage == buffer_usage_t::indirect)
            debug_name = L"DX12 Indirect Buffer";

        DX12_NAME(_resource, debug_name);

        if (use_upload_shadow)
        {
            D3D12_HEAP_PROPERTIES upload_heap_props{ };
            upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
            upload_heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            upload_heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            upload_heap_props.CreationNodeMask = 1;
            upload_heap_props.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC upload_desc{ resource_desc };
            upload_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            DX12_CHECK(device->CreateCommittedResource(&upload_heap_props,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &upload_desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                       nullptr,
                                                       IID_PPV_ARGS(&_upload_resource)));
            DX12_NAME(_upload_resource,
                      info.usage == buffer_usage_t::shader_read
                          ? L"DX12 Shader-Read Upload Buffer"
                          : L"DX12 Storage Upload Buffer");

            _mapped_resource = _upload_resource;
            DX12_CHECK(_mapped_resource->Map(0, nullptr, &_mapped_ptr));
        }
        else if (use_direct_upload_heap || use_readback_heap)
        {
            _mapped_resource = _resource;
            DX12_CHECK(_mapped_resource->Map(0, nullptr, &_mapped_ptr));
        }

        if (info.initial_data)
        {
            if (!write(info.initial_data, info.size_bytes, 0))
                LOG_GRAPHICS_FATAL("Failed to initialize DX12 buffer contents");
        }
    }

    dx12_buffer_t::~dx12_buffer_t()
    {
        if (_mapped_resource)
        {
            _mapped_resource->Unmap(0, nullptr);
            _mapped_resource = nullptr;
            _mapped_ptr = nullptr;
        }

        if (_upload_resource)
        {
            _upload_resource->Release();
            _upload_resource = nullptr;
        }

        if (_resource)
        {
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
        _pending_upload = _upload_resource != nullptr;

        return true;
    }

    bool dx12_buffer_t::flush_pending_upload(ID3D12GraphicsCommandList* command_list) const
    {
        if (!_pending_upload)
            return true;

        if (!command_list || !_resource || !_upload_resource)
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::flush_pending_upload called without a valid upload-backed resource");
            return false;
        }

        if (!transition_to(command_list, D3D12_RESOURCE_STATE_COPY_DEST))
            return false;

        command_list->CopyBufferRegion(_resource, 0u, _upload_resource, 0u, size_bytes());
        _pending_upload = false;
        return true;
    }

    bool dx12_buffer_t::transition_to(ID3D12GraphicsCommandList* command_list, const D3D12_RESOURCE_STATES new_state) const
    {
        if (!_resource)
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::transition_to called without a resource");
            return false;
        }

        if (!command_list)
        {
            LOG_GRAPHICS_ERROR("dx12_buffer_t::transition_to called without a command list");
            return false;
        }

        if (_resource_state == new_state)
            return true;

        D3D12_RESOURCE_BARRIER barrier{ };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = _resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = _resource_state;
        barrier.Transition.StateAfter = new_state;
        command_list->ResourceBarrier(1, &barrier);

        _resource_state = new_state;
        return true;
    }
} // namespace carrot::rhi::dx12
