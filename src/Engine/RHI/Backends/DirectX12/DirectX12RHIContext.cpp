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

#include <algorithm>

namespace carrot::rhi::dx12 {
    namespace {
        [[nodiscard]] constexpr uint32_t align_constant_buffer_size(const uint32_t size_bytes) noexcept
        {
            return (size_bytes + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u) &
                   ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u);
        }
    }

    dx12_rhi_context_t::dx12_rhi_context_t(const rhi_desc_t& desc)
    {
        _shader_files = desc.shader_files;
        if (core::platform::current_platform() != core::platform::platform_type::win32)
            LOG_GRAPHICS_FATAL("DX12 backend requires Win32 platform");

        const window::window_id_t presentation_window_id{
            window::has_window(desc.presentation_window_id)
                ? desc.presentation_window_id
                : window::get_main_window_id()
        };
        _presentation_window_id = presentation_window_id;
        core::platform::native_window_handle_t window{ window::get_native_handle(presentation_window_id) };

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

            DX12_CHECK(
                _device->id3d12_device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.
                    allocator)));

            DX12_NAME_INDEXED(frame.allocator, i, L"DX12 Frame Command Allocator");

            frame.command_list = std::make_unique<dx12_command_list_t>(_device->id3d12_device(), frame.allocator);
            frame.fence = std::make_unique<dx12_fence_t>(_device->id3d12_device());
            frame.fence_value = 0;

            buffer_create_info_t camera_buffer_info{ };
            camera_buffer_info.size_bytes = align_constant_buffer_size(sizeof(renderer::world_forward_plus_uniform_t));
            camera_buffer_info.usage = buffer_usage_t::uniform;
            camera_buffer_info.cpu_writable = true;

            for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_slots_per_frame; ++stage_slot)
            {
                frame.textured_quad_camera_uniform_buffers[stage_slot] = std::make_unique<dx12_buffer_t>(
                    _device->id3d12_device(),
                    camera_buffer_info
                );
            }
        }

        _srv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        _sampler_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        _textured_quad_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/textured_quad.vert.dxil",
            "engine://shaders/dx12/textured_quad.frag.dxil");
        _text_quad_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/text_quad.vert.dxil",
            "engine://shaders/dx12/text_quad.frag.dxil");

        if (!_textured_quad_pipeline || !_textured_quad_pipeline->is_valid())
            LOG_GRAPHICS_FATAL("Failed to create DX12 textured quad pipeline");
        if (!_text_quad_pipeline || !_text_quad_pipeline->is_valid())
            LOG_GRAPHICS_FATAL("Failed to create DX12 text quad pipeline");

        D3D12_INDIRECT_ARGUMENT_DESC draw_indexed_argument{ };
        draw_indexed_argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC signature_desc{ };
        signature_desc.ByteStride = sizeof(indexed_indirect_draw_command_t);
        signature_desc.NumArgumentDescs = 1u;
        signature_desc.pArgumentDescs = &draw_indexed_argument;

        DX12_CHECK(_device->id3d12_device()->CreateCommandSignature(&signature_desc,
                                                                    nullptr,
                                                                    IID_PPV_ARGS(&_draw_indexed_indirect_signature)));
        DX12_NAME(_draw_indexed_indirect_signature, L"DX12 DrawIndexed Indirect Signature");

        const std::uint32_t zero_value{ 0u };
        _default_compute_storage_buffer = create_buffer({
            .size_bytes = sizeof(zero_value),
            .usage = buffer_usage_t::storage,
            .initial_data = &zero_value
        });

        _rtv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    dx12_rhi_context_t::~dx12_rhi_context_t()
    {
        wait_idle();
        for (auxiliary_surface_t& surface : _auxiliary_surfaces)
            destroy_auxiliary_surface(surface);
        _auxiliary_surfaces.clear();

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            for (auto& srv_heap: frame.textured_quad_srv_heaps)
            {
                if (srv_heap)
                {
                    srv_heap->Release();
                    srv_heap = nullptr;
                }
            }

            for (auto& sampler_heap: frame.textured_quad_sampler_heaps)
            {
                if (sampler_heap)
                {
                    sampler_heap->Release();
                    sampler_heap = nullptr;
                }
            }

            if (frame.compute_uav_heap)
            {
                frame.compute_uav_heap->Release();
                frame.compute_uav_heap = nullptr;
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

        if (_draw_indexed_indirect_signature)
        {
            _draw_indexed_indirect_signature->Release();
            _draw_indexed_indirect_signature = nullptr;
        }

        _device.reset();
    }

    void dx12_rhi_context_t::begin_frame()
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        frame.fence->wait(frame.fence_value);
        frame.transient_compute_constant_buffers.clear();
        _recorded_stages.clear();
        _recorded_indirect_stages.clear();
        sync_auxiliary_surface_sizes();

        DX12_CHECK(frame.allocator->Reset());
        frame.command_list->set_allocator(frame.allocator);
        frame.command_list->reset();
        frame.command_list->begin_recording();

        _swapchain->acquire_next_image(nullptr);

        ID3D12GraphicsCommandList* cmd{ frame.command_list->id3d12_graphics_command_list() };
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
    }

    void dx12_rhi_context_t::record_quad_stage_to_active_target(const textured_quad_stage_record_t& stage,
                                                                const uint32_t stage_slot,
                                                                const quad_pipeline_kind_t pipeline_kind)
    {
        ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
        dx12_textured_quad_pipeline_t* pipeline{
            pipeline_kind == quad_pipeline_kind_t::text ? _text_quad_pipeline.get() : _textured_quad_pipeline.get()
        };

        if (pipeline &&
            pipeline->is_valid() &&
            stage.vertex_buffer != nullptr &&
            stage.index_buffer != nullptr &&
            !stage.batches.empty())
        {
            if (stage_slot >= k_max_textured_quad_stage_slots_per_frame)
            {
                LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                                   stage_slot,
                                   k_max_textured_quad_stage_slots_per_frame);
                return;
            }

            D3D12_VIEWPORT viewport{ };
            viewport.TopLeftX = static_cast<float>(stage.viewport.rect_px.position.x);
            viewport.TopLeftY = static_cast<float>(stage.viewport.rect_px.position.y);
            viewport.Width = static_cast<float>(stage.viewport.rect_px.size.x);
            viewport.Height = static_cast<float>(stage.viewport.rect_px.size.y);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            D3D12_RECT scissor{ };
            scissor.left = static_cast<LONG>(stage.viewport.rect_px.position.x);
            scissor.top = static_cast<LONG>(stage.viewport.rect_px.position.y);
            scissor.right = static_cast<LONG>(stage.viewport.rect_px.position.x + stage.viewport.rect_px.size.x);
            scissor.bottom = static_cast<LONG>(stage.viewport.rect_px.position.y + stage.viewport.rect_px.size.y);

            cmd->RSSetViewports(1, &viewport);
            cmd->RSSetScissorRects(1, &scissor);

            ensure_textured_quad_descriptor_capacity(static_cast<uint32_t>(stage.batches.size()));

            const dx12_frame_t& frame{ _frames[_frame_index] };

            renderer::world_forward_plus_uniform_t world_uniform{ };
            world_uniform.view_projection = stage.view_projection;
            world_uniform.ambient_color = stage.ambient_color;
            world_uniform.forward_plus_grid_params = stage.forward_plus_grid_params;
            world_uniform.forward_plus_tile_counts = stage.forward_plus_tile_counts;
            world_uniform.point_light_counts[0] = stage.point_light_count;
            world_uniform.point_lights = stage.point_lights;
            world_uniform.forward_plus_tiles = stage.forward_plus_tiles;
            world_uniform.forward_plus_light_indices = stage.forward_plus_light_indices;

            if (!frame.textured_quad_camera_uniform_buffers[stage_slot] ||
                !frame.textured_quad_camera_uniform_buffers[stage_slot]->write(&world_uniform,
                                                                               sizeof(world_uniform), 0))
            {
                LOG_GRAPHICS_FATAL("Failed to upload DX12 world forward+ uniform");
                return;
            }

            const draw_context_t draw_context{
                .command_list = cmd,
                .viewport = stage.viewport,
                .vertex_buffer = stage.vertex_buffer,
                .index_buffer = stage.index_buffer,
                .batches = stage.batches
            };

            const descriptor_context_t descriptor_context{
                .tables{
                    .srv_heap = frame.textured_quad_srv_heaps[stage_slot],
                    .srv_descriptor_size = _srv_descriptor_stride,
                    .camera_cbv_handle = frame.textured_quad_srv_heaps[stage_slot]->GetGPUDescriptorHandleForHeapStart(),
                    .first_batch_srv_index = 1,
                    .sampler_heap = frame.textured_quad_sampler_heaps[stage_slot],
                    .sampler_descriptor_size = _sampler_descriptor_stride
                },
                .sampler_provider = this
            };

            pipeline->draw(draw_context, descriptor_context);
        }
    }

    void dx12_rhi_context_t::record_textured_quad_stage(const textured_quad_stage_record_t& stage)
    {
        _recorded_stages.push_back({
            .stage = stage,
            .stage_slot = static_cast<uint32_t>(_recorded_stages.size()),
            .pipeline_kind = quad_pipeline_kind_t::textured
        });
        if (_recorded_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_stages.pop_back();
            return;
        }
        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
            record_quad_stage_to_active_target(stage, _recorded_stages.back().stage_slot, quad_pipeline_kind_t::textured);
    }

    void dx12_rhi_context_t::record_text_quad_stage(const textured_quad_stage_record_t& stage)
    {
        _recorded_stages.push_back({
            .stage = stage,
            .stage_slot = static_cast<uint32_t>(_recorded_stages.size()),
            .pipeline_kind = quad_pipeline_kind_t::text
        });
        if (_recorded_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_stages.pop_back();
            return;
        }
        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
            record_quad_stage_to_active_target(stage, _recorded_stages.back().stage_slot, quad_pipeline_kind_t::text);
    }

    void dx12_rhi_context_t::record_indirect_textured_quad_stage(const indirect_textured_quad_stage_record_t& stage)
    {
        const uint32_t stage_slot{ static_cast<uint32_t>(_recorded_stages.size() + _recorded_indirect_stages.size()) };
        if (stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            return;
        }

        _recorded_indirect_stages.push_back({
            .stage = stage,
            .stage_slot = stage_slot
        });

        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
            record_indirect_textured_quad_stage_to_active_target(stage, stage_slot);
    }

    void dx12_rhi_context_t::end_frame()
    {
        auto& f{ _frames[_frame_index] };
        ID3D12GraphicsCommandList* cmd{ f.command_list->id3d12_graphics_command_list() };
        const dx12_swapchain_t* sc{ _swapchain.get() };
        ID3D12Resource* backbuffer{ sc->get_backbuffer(sc->get_current_image_index()) };
        std::vector<dx12_swapchain_t*> present_aux_swapchains;
        const bool allow_auxiliary_presentation{ !window::is_fullscreen(_presentation_window_id) };

        for (auxiliary_surface_t& surface : _auxiliary_surfaces)
        {
            if (!allow_auxiliary_presentation)
                break;

            if (!surface.swapchain)
                continue;

            surface.swapchain->acquire_next_image(nullptr);
            const uint32_t aux_image_index{ surface.swapchain->get_current_image_index() };
            ID3D12Resource* aux_backbuffer{ surface.swapchain->get_backbuffer(aux_image_index) };
            const D3D12_CPU_DESCRIPTOR_HANDLE aux_rtv{ surface.swapchain->get_current_rtv(_rtv_descriptor_stride) };

            D3D12_RESOURCE_BARRIER aux_to_rtv{ };
            aux_to_rtv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            aux_to_rtv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            aux_to_rtv.Transition.pResource = aux_backbuffer;
            aux_to_rtv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            aux_to_rtv.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            aux_to_rtv.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmd->ResourceBarrier(1, &aux_to_rtv);

            float clear[]{ 0.02f, 0.02f, 0.04f, 1.0f };
            cmd->OMSetRenderTargets(1, &aux_rtv, FALSE, nullptr);
            cmd->ClearRenderTargetView(aux_rtv, clear, 0, nullptr);

            for (const recorded_stage_t& recorded_stage : _recorded_stages)
            {
                const textured_quad_stage_record_t& stage{ recorded_stage.stage };
                if (!presentation_mask_includes(stage.presentation_mask, surface.presentation_channel_mask))
                    continue;
                record_quad_stage_to_active_target(stage, recorded_stage.stage_slot, recorded_stage.pipeline_kind);
            }
            for (const recorded_indirect_stage_t& recorded_stage : _recorded_indirect_stages)
            {
                const indirect_textured_quad_stage_record_t& stage{ recorded_stage.stage };
                if (!presentation_mask_includes(stage.presentation_mask, surface.presentation_channel_mask))
                    continue;
                record_indirect_textured_quad_stage_to_active_target(stage, recorded_stage.stage_slot);
            }

            D3D12_RESOURCE_BARRIER aux_to_present{ };
            aux_to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            aux_to_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            aux_to_present.Transition.pResource = aux_backbuffer;
            aux_to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            aux_to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            aux_to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            cmd->ResourceBarrier(1, &aux_to_present);

            present_aux_swapchains.push_back(surface.swapchain.get());
        }

        D3D12_RESOURCE_BARRIER to_present{ };
        to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_present.Transition.pResource = backbuffer;
        to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmd->ResourceBarrier(1, &to_present);

        f.command_list->end_recording();

        _graphics_queue->submit(f.command_list.get(), f.fence.get(), nullptr, nullptr);
        f.fence_value = f.fence->current_value();
        _swapchain->present(nullptr);
        for (dx12_swapchain_t* aux_swapchain : present_aux_swapchains)
            aux_swapchain->present(nullptr);

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

    bool dx12_rhi_context_t::add_presentation_window(const window::window_id_t window_id,
                                                     const uint32_t presentation_channel_mask)
    {
        if (window_id == window::invalid_window_id || window_id == _presentation_window_id)
            return false;

        const bool already_registered{
            std::find_if(_auxiliary_surfaces.begin(),
                         _auxiliary_surfaces.end(),
                         [window_id](const auxiliary_surface_t& surface) { return surface.id == window_id; }) != _auxiliary_surfaces.end()
        };
        if (already_registered)
            return true;

        return create_auxiliary_surface(window_id, presentation_channel_mask);
    }

    bool dx12_rhi_context_t::remove_presentation_window(const window::window_id_t window_id)
    {
        if (window_id == window::invalid_window_id)
            return false;

        auto it{
            std::find_if(_auxiliary_surfaces.begin(),
                         _auxiliary_surfaces.end(),
                         [window_id](const auxiliary_surface_t& surface) { return surface.id == window_id; })
        };
        if (it == _auxiliary_surfaces.end())
            return false;

        wait_idle();
        destroy_auxiliary_surface(*it);
        _auxiliary_surfaces.erase(it);
        return true;
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
        DX12_NAME(texture, L"DX12 Texture2D");

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
        DX12_NAME(upload_buffer, L"DX12 Texture Upload Buffer");

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

        DX12_NAME(allocator, L"DX12 Texture Upload Command Allocator");
        DX12_NAME(cmd, L"DX12 Texture Upload Command List");

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

    std::unique_ptr<rhi_compute_pipeline_t> dx12_rhi_context_t::create_compute_pipeline(
        const compute_pipeline_create_info_t& info)
    {
        if (!_device || !_shader_files)
            return nullptr;

        return std::make_unique<dx12_compute_pipeline_t>(_device->id3d12_device(), *_shader_files, info);
    }

    std::unique_ptr<rhi_sampler_t> dx12_rhi_context_t::create_sampler(const sampler_desc_t& desc) const
    {
        return std::make_unique<dx12_sampler_t>(desc);
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

    void dx12_rhi_context_t::dispatch_compute(const compute_dispatch_record_t& record)
    {
        if (!record.pipeline)
            return;

        if (record.order != compute_dispatch_order_t::before_graphics)
        {
            LOG_GRAPHICS_ERROR("DX12 compute dispatch received unsupported dispatch order");
            return;
        }

        if (!_recorded_stages.empty())
        {
            LOG_GRAPHICS_ERROR("DX12 compute dispatch currently must happen before graphics stage recording");
            return;
        }

        const auto* pipeline{ dynamic_cast<const dx12_compute_pipeline_t*>(record.pipeline) };
        if (!pipeline || !pipeline->is_valid())
        {
            LOG_GRAPHICS_ERROR("dispatch_compute received non-DX12 or invalid compute pipeline");
            return;
        }

        if (record.constants.size() > pipeline->info().max_constant_size_bytes)
        {
            LOG_GRAPHICS_ERROR("DX12 compute constants exceed pipeline limit");
            return;
        }

        ensure_compute_descriptor_capacity();

        dx12_frame_t& frame{ _frames[_frame_index] };
        ID3D12GraphicsCommandList* cmd{ frame.command_list->id3d12_graphics_command_list() };
        if (!cmd || !frame.compute_uav_heap)
            return;

        const auto* default_buffer{ dynamic_cast<const dx12_buffer_t*>(_default_compute_storage_buffer.get()) };
        if (!default_buffer)
        {
            LOG_GRAPHICS_ERROR("DX12 default compute storage buffer is not available");
            return;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE heap_start{ frame.compute_uav_heap->GetCPUDescriptorHandleForHeapStart() };
        for (std::uint32_t slot{ 0u }; slot < k_max_compute_storage_buffer_bindings; ++slot)
        {
            const rhi_buffer_t* bound_buffer{ _default_compute_storage_buffer.get() };
            for (const compute_buffer_binding_t& binding : record.storage_buffers)
            {
                if (binding.slot == slot && binding.buffer)
                {
                    bound_buffer = binding.buffer;
                    break;
                }
            }

            const auto* dx_buffer{ dynamic_cast<const dx12_buffer_t*>(bound_buffer) };
            if (!dx_buffer)
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch received invalid storage buffer");
                return;
            }

            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{ };
            uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.FirstElement = 0u;
            uav_desc.Buffer.NumElements = static_cast<UINT>(std::max<std::size_t>(1u, dx_buffer->size_bytes() / 4u));
            uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

            D3D12_CPU_DESCRIPTOR_HANDLE handle{ heap_start };
            handle.ptr += static_cast<SIZE_T>(slot) * _srv_descriptor_stride;
            _device->id3d12_device()->CreateUnorderedAccessView(dx_buffer->resource(), nullptr, &uav_desc, handle);
        }

        if (record.graphics_handoff == compute_graphics_handoff_t::storage_write_to_graphics_read)
        {
            std::vector<D3D12_RESOURCE_BARRIER> to_uav_barriers;
            to_uav_barriers.reserve(record.storage_buffers.size());

            for (const compute_buffer_binding_t& binding : record.storage_buffers)
            {
                if (!binding.buffer)
                    continue;

                const auto* dx_buffer{ dynamic_cast<const dx12_buffer_t*>(binding.buffer) };
                if (!dx_buffer)
                    continue;

                D3D12_RESOURCE_BARRIER barrier{ };
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = dx_buffer->resource();
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                to_uav_barriers.push_back(barrier);
            }

            if (!to_uav_barriers.empty())
                cmd->ResourceBarrier(static_cast<UINT>(to_uav_barriers.size()), to_uav_barriers.data());
        }

        ID3D12DescriptorHeap* heaps[]{ frame.compute_uav_heap };
        cmd->SetComputeRootSignature(pipeline->root_signature());
        cmd->SetPipelineState(pipeline->pipeline_state());
        cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetComputeRootDescriptorTable(0, frame.compute_uav_heap->GetGPUDescriptorHandleForHeapStart());

        if (pipeline->info().max_constant_size_bytes > 0u)
        {
            const std::uint32_t constant_buffer_size{
                align_constant_buffer_size(std::max<std::uint32_t>(
                    static_cast<std::uint32_t>(record.constants.size()),
                    pipeline->info().max_constant_size_bytes))
            };

            buffer_create_info_t constant_buffer_info{ };
            constant_buffer_info.size_bytes = constant_buffer_size;
            constant_buffer_info.usage = buffer_usage_t::uniform;
            constant_buffer_info.cpu_writable = true;

            auto constant_buffer{
                std::make_unique<dx12_buffer_t>(_device->id3d12_device(), constant_buffer_info)
            };

            if (!record.constants.empty() &&
                !constant_buffer->write(record.constants.data(), record.constants.size(), 0u))
            {
                LOG_GRAPHICS_ERROR("Failed to upload DX12 compute constants");
                return;
            }

            cmd->SetComputeRootConstantBufferView(1, constant_buffer->resource()->GetGPUVirtualAddress());
            frame.transient_compute_constant_buffers.push_back(std::move(constant_buffer));
        }

        cmd->Dispatch(record.group_count_x, record.group_count_y, record.group_count_z);

        if (record.graphics_handoff == compute_graphics_handoff_t::storage_write_to_graphics_read)
        {
            D3D12_RESOURCE_BARRIER uav_barrier{ };
            uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            std::vector<D3D12_RESOURCE_BARRIER> to_common_barriers;
            to_common_barriers.reserve(record.storage_buffers.size() + 1u);

            for (const compute_buffer_binding_t& binding : record.storage_buffers)
            {
                if (!binding.buffer)
                    continue;

                const auto* dx_buffer{ dynamic_cast<const dx12_buffer_t*>(binding.buffer) };
                if (!dx_buffer)
                    continue;

                D3D12_RESOURCE_BARRIER compute_uav_barrier{ uav_barrier };
                compute_uav_barrier.UAV.pResource = dx_buffer->resource();
                to_common_barriers.push_back(compute_uav_barrier);

                D3D12_RESOURCE_BARRIER transition{ };
                transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                transition.Transition.pResource = dx_buffer->resource();
                transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
                to_common_barriers.push_back(transition);
            }

            if (!to_common_barriers.empty())
                cmd->ResourceBarrier(static_cast<UINT>(to_common_barriers.size()), to_common_barriers.data());
        }
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

    void dx12_rhi_context_t::sync_auxiliary_surface_sizes()
    {
        for (auto it = _auxiliary_surfaces.begin(); it != _auxiliary_surfaces.end();)
        {
            if (!window::has_window(it->id))
            {
                wait_idle();
                destroy_auxiliary_surface(*it);
                it = _auxiliary_surfaces.erase(it);
                continue;
            }

            const uint32_t width{ window::get_width(it->id) };
            const uint32_t height{ window::get_height(it->id) };
            if (width > 0 && height > 0 && (width != it->last_width || height != it->last_height))
            {
                wait_idle();
                it->swapchain->resize(width, height);
                it->last_width = width;
                it->last_height = height;
            }

            ++it;
        }
    }

    bool dx12_rhi_context_t::create_auxiliary_surface(const window::window_id_t window_id,
                                                      const uint32_t presentation_channel_mask)
    {
        if (!_device || !_graphics_queue || !window::has_window(window_id))
            return false;

        const core::platform::native_window_handle_t window_handle{ window::get_native_handle(window_id) };
        const HWND hwnd{ static_cast<HWND>(window_handle.win32_t.hwnd) };
        const uint32_t width{ window::get_width(window_id) };
        const uint32_t height{ window::get_height(window_id) };
        if (!hwnd || width == 0 || height == 0)
            return false;

        auxiliary_surface_t surface{ };
        surface.id = window_id;
        surface.presentation_channel_mask = presentation_channel_mask;
        surface.last_width = width;
        surface.last_height = height;
        surface.swapchain = std::make_unique<dx12_swapchain_t>(_device->id3d12_device(),
                                                                _graphics_queue->id3d12_command_queue(),
                                                                hwnd,
                                                                width,
                                                                height);
        _auxiliary_surfaces.push_back(std::move(surface));
        return true;
    }

    void dx12_rhi_context_t::destroy_auxiliary_surface(auxiliary_surface_t& surface) noexcept
    {
        surface.swapchain.reset();
        surface.id = window::invalid_window_id;
        surface.last_width = 0;
        surface.last_height = 0;
    }

    void dx12_rhi_context_t::record_indirect_textured_quad_stage_to_active_target(
        const indirect_textured_quad_stage_record_t& stage,
        const uint32_t stage_slot)
    {
        ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
        if (!_textured_quad_pipeline ||
            !_textured_quad_pipeline->is_valid() ||
            !_draw_indexed_indirect_signature ||
            !stage.vertex_buffer ||
            !stage.index_buffer ||
            !stage.indirect_buffer ||
            !stage.texture ||
            !stage.sampler)
        {
            return;
        }

        if (stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            return;
        }

        ensure_indirect_textured_quad_descriptor_capacity(1u);

        const dx12_frame_t& frame{ _frames[_frame_index] };

        renderer::world_forward_plus_uniform_t world_uniform{ };
        world_uniform.view_projection = stage.view_projection;
        world_uniform.ambient_color = stage.ambient_color;
        world_uniform.forward_plus_grid_params = stage.forward_plus_grid_params;
        world_uniform.forward_plus_tile_counts = stage.forward_plus_tile_counts;
        world_uniform.point_light_counts[0] = stage.point_light_count;
        world_uniform.point_lights = stage.point_lights;
        world_uniform.forward_plus_tiles = stage.forward_plus_tiles;
        world_uniform.forward_plus_light_indices = stage.forward_plus_light_indices;

        if (!frame.textured_quad_camera_uniform_buffers[stage_slot] ||
            !frame.textured_quad_camera_uniform_buffers[stage_slot]->write(&world_uniform, sizeof(world_uniform), 0))
        {
            LOG_GRAPHICS_FATAL("Failed to upload DX12 world forward+ uniform for indirect stage");
            return;
        }

        const indirect_draw_context_t draw_context{
            .command_list = cmd,
            .draw_indexed_indirect_signature = _draw_indexed_indirect_signature,
            .viewport = stage.viewport,
            .vertex_buffer = stage.vertex_buffer,
            .index_buffer = stage.index_buffer,
            .indirect_buffer = stage.indirect_buffer,
            .indirect_buffer_offset_bytes = stage.indirect_buffer_offset_bytes,
            .texture = stage.texture,
            .sampler = stage.sampler
        };

        const descriptor_context_t descriptor_context{
            .tables{
                .srv_heap = frame.indirect_textured_quad_srv_heaps[stage_slot],
                .srv_descriptor_size = _srv_descriptor_stride,
                .camera_cbv_handle = frame.indirect_textured_quad_srv_heaps[stage_slot]->GetGPUDescriptorHandleForHeapStart(),
                .first_batch_srv_index = 1u,
                .sampler_heap = frame.indirect_textured_quad_sampler_heaps[stage_slot],
                .sampler_descriptor_size = _sampler_descriptor_stride,
                .first_batch_sampler_index = 0u
            },
            .sampler_provider = this
        };

        const auto* dx_indirect_buffer{ dynamic_cast<const dx12_buffer_t*>(stage.indirect_buffer) };
        if (!dx_indirect_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad stage received non-DX12 indirect buffer");
            return;
        }

        D3D12_HEAP_PROPERTIES heap_properties{ };
        D3D12_HEAP_FLAGS heap_flags{ D3D12_HEAP_FLAG_NONE };
        const bool can_transition_indirect_buffer{
            dx_indirect_buffer->resource() &&
            SUCCEEDED(dx_indirect_buffer->resource()->GetHeapProperties(&heap_properties, &heap_flags)) &&
            heap_properties.Type == D3D12_HEAP_TYPE_DEFAULT
        };

        if (can_transition_indirect_buffer)
        {
            D3D12_RESOURCE_BARRIER to_indirect{ };
            to_indirect.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            to_indirect.Transition.pResource = dx_indirect_buffer->resource();
            to_indirect.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            to_indirect.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            to_indirect.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            cmd->ResourceBarrier(1, &to_indirect);
        }

        _textured_quad_pipeline->draw_indirect(draw_context, descriptor_context);

        if (can_transition_indirect_buffer)
        {
            D3D12_RESOURCE_BARRIER to_common{ };
            to_common.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            to_common.Transition.pResource = dx_indirect_buffer->resource();
            to_common.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            to_common.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            to_common.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            cmd->ResourceBarrier(1, &to_common);
        }
    }

    void dx12_rhi_context_t::ensure_textured_quad_descriptor_capacity(const uint32_t required_capacity)
    {
        if (required_capacity == 0)
            return;

        const uint32_t target_capacity{ std::max(required_capacity, 16u) };
        const uint32_t srv_heap_descriptor_count{ target_capacity + 1u };

        for (uint32_t frame_index{ 0 }; frame_index < k_max_frames_in_flight; ++frame_index)
        {
            dx12_frame_t& frame{ _frames[frame_index] };
            bool has_all_srv_heaps{ true };
            bool has_all_sampler_heaps{ true };

            for (const ID3D12DescriptorHeap* heap: frame.textured_quad_srv_heaps)
                has_all_srv_heaps = has_all_srv_heaps && (heap != nullptr);

            for (const ID3D12DescriptorHeap* heap: frame.textured_quad_sampler_heaps)
                has_all_sampler_heaps = has_all_sampler_heaps && (heap != nullptr);

            if (frame.textured_quad_descriptor_capacity >= target_capacity &&
                has_all_srv_heaps &&
                has_all_sampler_heaps)
            {
                continue;
            }

            frame.fence->wait(frame.fence_value);

            for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_slots_per_frame; ++stage_slot)
            {
                if (frame.textured_quad_srv_heaps[stage_slot])
                {
                    frame.textured_quad_srv_heaps[stage_slot]->Release();
                    frame.textured_quad_srv_heaps[stage_slot] = nullptr;
                }

                if (frame.textured_quad_sampler_heaps[stage_slot])
                {
                    frame.textured_quad_sampler_heaps[stage_slot]->Release();
                    frame.textured_quad_sampler_heaps[stage_slot] = nullptr;
                }

                D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{ };
                srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                srv_heap_desc.NumDescriptors = srv_heap_descriptor_count;
                srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                srv_heap_desc.NodeMask = 0;

                DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&srv_heap_desc,
                    IID_PPV_ARGS(&frame.textured_quad_srv_heaps[stage_slot])));

                D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{ };
                sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                sampler_heap_desc.NumDescriptors = target_capacity;
                sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                sampler_heap_desc.NodeMask = 0;

                DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&sampler_heap_desc,
                    IID_PPV_ARGS(&frame.textured_quad_sampler_heaps[stage_slot])));

                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{ };
                cbv_desc.BufferLocation =
                    frame.textured_quad_camera_uniform_buffers[stage_slot]->resource()->GetGPUVirtualAddress();
                cbv_desc.SizeInBytes = align_constant_buffer_size(sizeof(renderer::world_forward_plus_uniform_t));

                _device->id3d12_device()->CreateConstantBufferView(
                    &cbv_desc,
                    frame.textured_quad_srv_heaps[stage_slot]->GetCPUDescriptorHandleForHeapStart()
                );
            }

            frame.textured_quad_descriptor_capacity = target_capacity;
        }
    }

    void dx12_rhi_context_t::ensure_indirect_textured_quad_descriptor_capacity([[maybe_unused]] const uint32_t required_capacity)
    {
        for (uint32_t frame_index{ 0 }; frame_index < k_max_frames_in_flight; ++frame_index)
        {
            dx12_frame_t& frame{ _frames[frame_index] };
            frame.fence->wait(frame.fence_value);

            for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_slots_per_frame; ++stage_slot)
            {
                if (!frame.indirect_textured_quad_srv_heaps[stage_slot])
                {
                    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{ };
                    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    srv_heap_desc.NumDescriptors = 2u;
                    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                    srv_heap_desc.NodeMask = 0;

                    DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(
                        &srv_heap_desc,
                        IID_PPV_ARGS(&frame.indirect_textured_quad_srv_heaps[stage_slot])));

                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{ };
                    cbv_desc.BufferLocation =
                        frame.textured_quad_camera_uniform_buffers[stage_slot]->resource()->GetGPUVirtualAddress();
                    cbv_desc.SizeInBytes = align_constant_buffer_size(sizeof(renderer::world_forward_plus_uniform_t));

                    _device->id3d12_device()->CreateConstantBufferView(
                        &cbv_desc,
                        frame.indirect_textured_quad_srv_heaps[stage_slot]->GetCPUDescriptorHandleForHeapStart()
                    );
                }

                if (!frame.indirect_textured_quad_sampler_heaps[stage_slot])
                {
                    D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{ };
                    sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                    sampler_heap_desc.NumDescriptors = 1u;
                    sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                    sampler_heap_desc.NodeMask = 0;

                    DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(
                        &sampler_heap_desc,
                        IID_PPV_ARGS(&frame.indirect_textured_quad_sampler_heaps[stage_slot])));
                }
            }
        }
    }

    void dx12_rhi_context_t::ensure_compute_descriptor_capacity()
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        if (frame.compute_uav_heap)
            return;

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = k_max_compute_storage_buffer_bindings;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&frame.compute_uav_heap)));
        DX12_NAME(frame.compute_uav_heap, L"DX12 Compute UAV Heap");
    }
} // namespace carrot::rhi::dx12
