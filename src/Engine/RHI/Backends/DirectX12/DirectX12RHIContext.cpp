//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12RHIContext.h"

#include "DirectX12CommandList.h"
#include "DirectX12CommandQueue.h"
#include "DirectX12Device.h"
#include "DirectX12Fence.h"
#include "DirectX12Swapchain.h"
#include "RHI/RHI.h"
#include "Utils/File/FileUtils.h"
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

        _srv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        _sampler_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        _textured_quad_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(), *desc.shader_files);

        if (!_textured_quad_pipeline || !_textured_quad_pipeline->is_valid())
            LOG_GRAPHICS_FATAL("Failed to create DX12 textured quad pipeline");

        _rtv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    dx12_rhi_context_t::~dx12_rhi_context_t()
    {
        wait_idle();

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            if (frame.textured_quad_srv_heap)
            {
                frame.textured_quad_srv_heap->Release();
                frame.textured_quad_srv_heap = nullptr;
            }

            if (frame.textured_quad_sampler_heap)
            {
                frame.textured_quad_sampler_heap->Release();
                frame.textured_quad_sampler_heap = nullptr;
            }

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
        const dx12_swapchain_t* sc{ _swapchain.get() };

        float clear[]{ 0.02f, 0.02f, 0.04f, 1.0f };

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ sc->get_current_rtv(_rtv_descriptor_stride) };
        ID3D12Resource* backbuffer{ sc->get_backbuffer(sc->get_current_image_index()) };

        D3D12_RESOURCE_BARRIER to_rtv{ };
        to_rtv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_rtv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_rtv.Transition.pResource = backbuffer;
        to_rtv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_rtv.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        to_rtv.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        cmd->ResourceBarrier(1, &to_rtv);

        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

        D3D12_VIEWPORT viewport{ };
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(sc->get_width());
        viewport.Height = static_cast<float>(sc->get_height());
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor{ };
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = static_cast<LONG>(sc->get_width());
        scissor.bottom = static_cast<LONG>(sc->get_height());

        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);

        if (_textured_quad_pipeline &&
            _textured_quad_pipeline->is_valid() &&
            _textured_quad_vertex_buffer != nullptr &&
            _textured_quad_index_buffer != nullptr &&
            !_textured_quad_batches.empty())
        {
            ensure_textured_quad_descriptor_capacity(static_cast<uint32_t>(_textured_quad_batches.size()));

            const dx12_frame_t& frame{ _frames[_frame_index] };

            const draw_context_t draw_context{
                .command_list = cmd,
                .render_width = sc->get_width(),
                .render_height = sc->get_height(),
                .vertex_buffer = _textured_quad_vertex_buffer,
                .index_buffer = _textured_quad_index_buffer,
                .batches = _textured_quad_batches
            };

            const descriptor_context_t descriptor_context{
                .tables{
                    .srv_heap = frame.textured_quad_srv_heap,
                    .srv_descriptor_size = _srv_descriptor_stride,
                    .sampler_heap = frame.textured_quad_sampler_heap,
                    .sampler_descriptor_size = _sampler_descriptor_stride
                },
                .sampler_provider = this
            };

            _textured_quad_pipeline->draw(draw_context, descriptor_context);
        }

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

    void dx12_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        // Make sure GPU is not touching old backbuffers/RTVs.
        wait_idle();

        if (_swapchain)
        {
            _swapchain->resize(width, height);
        }
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

    std::unique_ptr<rhi_texture_t> dx12_rhi_context_t::create_texture_2d(const texture_create_info_t& info)
    {
        if (info.width == 0 || info.height == 0)
        {
            LOG_GRAPHICS_ERROR("DX12 create_texture_2d called with invalid dimensions {}x{}",
                               info.width, info.height);
            return nullptr;
        }

        if (!info.initial_data || info.initial_data_size == 0 || info.initial_data_stride_bytes == 0)
        {
            LOG_GRAPHICS_ERROR("DX12 create_texture_2d currently requires initial data");
            return nullptr;
        }

        ID3D12Device* device{ _device->id3d12_device() };

        const DXGI_FORMAT resource_format{ dx12_texture_resource_format(info.format) };
        const DXGI_FORMAT srv_format{ dx12_texture_srv_format(info.format) };

        D3D12_HEAP_PROPERTIES default_heap{ };
        default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        default_heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        default_heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        default_heap.CreationNodeMask = 1;
        default_heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC texture_desc{ };
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Alignment = 0;
        texture_desc.Width = info.width;
        texture_desc.Height = info.height;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.Format = resource_format;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* texture{ nullptr };
        DX12_CHECK(device->CreateCommittedResource(
            &default_heap,
            D3D12_HEAP_FLAG_NONE,
            &texture_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&texture)
        ));

        UINT64 upload_buffer_size{ 0 };
        device->GetCopyableFootprints(&texture_desc, 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size);

        D3D12_HEAP_PROPERTIES upload_heap{ };
        upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        upload_heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        upload_heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        upload_heap.CreationNodeMask = 1;
        upload_heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC upload_desc{ };
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Alignment = 0;
        upload_desc.Width = upload_buffer_size;
        upload_desc.Height = 1;
        upload_desc.DepthOrArraySize = 1;
        upload_desc.MipLevels = 1;
        upload_desc.Format = DXGI_FORMAT_UNKNOWN;
        upload_desc.SampleDesc.Count = 1;
        upload_desc.SampleDesc.Quality = 0;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        upload_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* upload_buffer{ nullptr };
        DX12_CHECK(device->CreateCommittedResource(
            &upload_heap,
            D3D12_HEAP_FLAG_NONE,
            &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&upload_buffer)
        ));

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{ };
        UINT num_rows{ 0 };
        UINT64 row_size_in_bytes{ 0 };
        UINT64 total_bytes{ 0 };
        device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &num_rows, &row_size_in_bytes, &total_bytes);

        void* mapped{ nullptr };
        DX12_CHECK(upload_buffer->Map(0, nullptr, &mapped));

        const std::byte* src_bytes{ static_cast<const std::byte*>(info.initial_data) };
        std::byte* dst_bytes{ static_cast<std::byte*>(mapped) + footprint.Offset };

        for (UINT row{ 0 }; row < num_rows; ++row)
        {
            std::memcpy(dst_bytes + row * footprint.Footprint.RowPitch,
                        src_bytes + row * info.initial_data_stride_bytes,
                        info.initial_data_stride_bytes);
        }

        upload_buffer->Unmap(0, nullptr);

        ID3D12CommandAllocator* allocator{ nullptr };
        ID3D12GraphicsCommandList* cmd{ nullptr };

        DX12_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        DX12_CHECK(device->CreateCommandList(0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator,
            nullptr,
            IID_PPV_ARGS(&cmd)));

        D3D12_TEXTURE_COPY_LOCATION dst{ };
        dst.pResource = texture;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src{ };
        src.pResource = upload_buffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER barrier{ };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        cmd->ResourceBarrier(1, &barrier);

        DX12_CHECK(cmd->Close());

        ID3D12CommandList* command_lists[]{ cmd };
        _graphics_queue->id3d12_command_queue()->ExecuteCommandLists(1, command_lists);

        auto upload_fence = std::make_unique<dx12_fence_t>(device);
        upload_fence->signal(_graphics_queue->id3d12_command_queue());
        upload_fence->wait();

        cmd->Release();
        allocator->Release();
        upload_buffer->Release();

        return std::make_unique<dx12_texture_t>(info.width,
                                                info.height,
                                                info.format,
                                                texture,
                                                resource_format,
                                                srv_format);
    }

    std::unique_ptr<rhi_buffer_t> dx12_rhi_context_t::create_buffer(const buffer_create_info_t& info)
    {
        return std::make_unique<dx12_buffer_t>(_device->id3d12_device(), info);
    }

    std::unique_ptr<rhi_sampler_t> dx12_rhi_context_t::create_sampler(const sampler_desc_t& desc) const
    {
        return std::make_unique<dx12_sampler_t>(desc);
    }

    void dx12_rhi_context_t::set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer,
                                                        const rhi_buffer_t& index_buffer)
    {
        _textured_quad_vertex_buffer = &vertex_buffer;
        _textured_quad_index_buffer = &index_buffer;
    }

    void dx12_rhi_context_t::set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches)
    {
        _textured_quad_batches.assign(batches.begin(), batches.end());
    }

    rhi_sampler_t* dx12_rhi_context_t::get_or_create_sampler(const sampler_desc_t& desc)
    {
        if (const auto it = _sampler_cache.find(desc); it != _sampler_cache.end())
            return it->second.get();

        auto sampler = std::make_unique<dx12_sampler_t>(desc);
        rhi_sampler_t* ptr = sampler.get();
        _sampler_cache.emplace(desc, std::move(sampler));

        return ptr;
    }

    void dx12_rhi_context_t::bind_textured_quad_resources([[maybe_unused]] const rhi_texture_t& texture,
                                                          [[maybe_unused]] const rhi_sampler_t& sampler)
    {
        // DX12 binding is performed during draw recording via descriptor heaps.
        // This is a valid no-op for now to satisfy the RHI contract cleanly.
    }

    void dx12_rhi_context_t::wait_idle()
    {
        if (_graphics_queue)
            _graphics_queue->wait_idle();

        // These should all be completed now, but we are being safe
        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
            _frames[i].fence->wait(_frames[i].fence_value);
    }

    // PRIVATE

    void dx12_rhi_context_t::ensure_textured_quad_descriptor_capacity(const uint32_t required_capacity)
    {
        if (required_capacity == 0)
            return;

        const uint32_t target_capacity{ std::max(required_capacity, 16u) };

        for (dx12_frame_t& frame: _frames)
        {
            if (frame.textured_quad_descriptor_capacity >= target_capacity &&
                frame.textured_quad_srv_heap != nullptr &&
                frame.textured_quad_sampler_heap != nullptr)
            {
                continue;
            }

            if (frame.textured_quad_srv_heap)
            {
                frame.textured_quad_srv_heap->Release();
                frame.textured_quad_srv_heap = nullptr;
            }

            if (frame.textured_quad_sampler_heap)
            {
                frame.textured_quad_sampler_heap->Release();
                frame.textured_quad_sampler_heap = nullptr;
            }

            D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{ };
            srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srv_heap_desc.NumDescriptors = target_capacity;
            srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            srv_heap_desc.NodeMask = 0;

            DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&srv_heap_desc,
                IID_PPV_ARGS(&frame.textured_quad_srv_heap)));

            D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{ };
            sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            sampler_heap_desc.NumDescriptors = target_capacity;
            sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            sampler_heap_desc.NodeMask = 0;

            DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&sampler_heap_desc,
                IID_PPV_ARGS(&frame.textured_quad_sampler_heap)));

            frame.textured_quad_descriptor_capacity = target_capacity;
        }
    }
} // namespace carrot::rhi::dx12
