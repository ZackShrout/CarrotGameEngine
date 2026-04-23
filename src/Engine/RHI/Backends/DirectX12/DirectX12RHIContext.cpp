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
#include <bit>

namespace carrot::rhi::dx12 {
    namespace {
        constexpr size_t k_dx12_upload_ring_capacity_bytes{ 64u * 1024u * 1024u };

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
        _upload_ring = std::make_unique<dx12_upload_ring_t>(_device->id3d12_device(), k_dx12_upload_ring_capacity_bytes);
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

        _instanced_textured_quad_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/textured_quad_instanced.vert.dxil",
            "engine://shaders/dx12/textured_quad.frag.dxil");
        _instanced_text_quad_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/text_quad_instanced.vert.dxil",
            "engine://shaders/dx12/text_quad.frag.dxil");
        _instanced_battle_swirl_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/textured_quad_instanced.vert.dxil",
            "engine://shaders/dx12/battle_swirl_transition.frag.dxil");
        _instanced_bloom_blur_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/textured_quad_instanced.vert.dxil",
            "engine://shaders/dx12/bloom_blur.frag.dxil");
        _instanced_bloom_composite_pipeline = std::make_unique<dx12_textured_quad_pipeline_t>(
            _device->id3d12_device(),
            *desc.shader_files,
            "engine://shaders/dx12/textured_quad_instanced.vert.dxil",
            "engine://shaders/dx12/bloom_composite.frag.dxil",
            dx12_textured_quad_pipeline_t::blend_mode_t::additive);
        if (!_instanced_textured_quad_pipeline || !_instanced_textured_quad_pipeline->is_valid() ||
            !_instanced_text_quad_pipeline || !_instanced_text_quad_pipeline->is_valid() ||
            !_instanced_battle_swirl_pipeline || !_instanced_battle_swirl_pipeline->is_valid() ||
            !_instanced_bloom_blur_pipeline || !_instanced_bloom_blur_pipeline->is_valid() ||
            !_instanced_bloom_composite_pipeline || !_instanced_bloom_composite_pipeline->is_valid())
        {
            LOG_GRAPHICS_FATAL("Failed to create DX12 M27 instanced quad pipelines");
        }

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

        _device.reset();
    }

    void dx12_rhi_context_t::begin_frame()
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        frame.fence->wait(frame.fence_value);
        frame.transient_compute_constant_buffers.clear();
        frame.compute_descriptor_count_used = 0u;
        _recorded_quad_stages.clear();
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
        if (stage.instance_buffer == nullptr || stage.instance_count == 0u)
        {
            LOG_GRAPHICS_FATAL("DX12 direct textured quad stage requires instanced draw data");
            return;
        }
        dx12_textured_quad_pipeline_t* pipeline{ resolve_quad_pipeline(pipeline_kind) };

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

            ensure_textured_quad_descriptor_capacity(stage_slot, static_cast<uint32_t>(stage.batches.size()));

            const dx12_frame_t& frame{ _frames[_frame_index] };

            const renderer::world_forward_plus_uniform_t world_uniform{
                renderer::pack_world_forward_plus_uniform(stage.view_projection,
                                                          stage.ambient_color,
                                                          stage.world_draw_mode,
                                                          stage.forward_plus_constants,
                                                          stage.forward_plus_light_input,
                                                          stage.forward_plus_output)
            };

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
                .instance_buffer = stage.instance_buffer,
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
                .sampler_provider = this,
                .forward_plus_light_input_buffer = stage.forward_plus_light_input_buffer
                                                       ? stage.forward_plus_light_input_buffer
                                                       : _default_compute_storage_buffer.get(),
                .forward_plus_output_buffer = stage.forward_plus_output_buffer
                                                  ? stage.forward_plus_output_buffer
                                                  : _default_compute_storage_buffer.get(),
                .world_item_buffer = stage.world_item_buffer
                                         ? stage.world_item_buffer
                                         : _default_compute_storage_buffer.get(),
                .visible_item_index_buffer = stage.visible_item_index_buffer
                                                 ? stage.visible_item_index_buffer
                                                 : _default_compute_storage_buffer.get()
            };

            pipeline->draw(draw_context, descriptor_context);
        }
    }

    void dx12_rhi_context_t::record_capture_textured_quad_stage_to_active_target(
        const textured_quad_stage_record_t& stage,
        const uint32_t stage_slot,
        const quad_pipeline_kind_t pipeline_kind,
        ID3D12Resource* const render_target,
        const D3D12_CPU_DESCRIPTOR_HANDLE& rtv)
    {
        auto* capture_texture{ dynamic_cast<const dx12_texture_t*>(stage.batches.empty() ? nullptr : stage.batches[0].texture) };
        if (!capture_texture || !render_target)
        {
            LOG_GRAPHICS_FATAL("DX12 battle swirl stage requires a valid capture texture and render target");
            return;
        }

        ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };

        D3D12_RESOURCE_BARRIER barriers[4]{ };
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = render_target;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = capture_texture->resource();
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        cmd->ResourceBarrier(2, barriers);
        cmd->CopyResource(capture_texture->resource(), render_target);

        barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[2].Transition.pResource = capture_texture->resource();
        barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[3].Transition.pResource = render_target;
        barriers[3].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[3].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        cmd->ResourceBarrier(2, &barriers[2]);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        record_quad_stage_to_active_target(stage, stage_slot, pipeline_kind);
    }

    void dx12_rhi_context_t::record_textured_quad_stage(const textured_quad_stage_record_t& stage)
    {
        const uint32_t stage_slot{ static_cast<uint32_t>(_recorded_quad_stages.size()) };
        _recorded_quad_stages.push_back({
            .kind = recorded_quad_stage_kind_t::direct,
            .stage_slot = stage_slot,
            .direct_stage = stage,
            .indirect_stage = { },
            .pipeline_kind = [&stage]() noexcept
            {
                switch (stage.shader_variant)
                {
                    case quad_shader_variant_t::battle_swirl: return quad_pipeline_kind_t::battle_swirl;
                    case quad_shader_variant_t::bloom_blur: return quad_pipeline_kind_t::bloom_blur;
                    case quad_shader_variant_t::bloom_composite: return quad_pipeline_kind_t::bloom_composite;
                    case quad_shader_variant_t::standard:
                    default: return quad_pipeline_kind_t::textured;
                }
            }()
        });
        recorded_quad_stage_t& recorded_stage{ _recorded_quad_stages.back() };
        recorded_stage.owned_direct_batches.assign(stage.batches.begin(), stage.batches.end());
        recorded_stage.direct_stage.batches = recorded_stage.owned_direct_batches;
        if (_recorded_quad_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_quad_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_quad_stages.pop_back();
            return;
        }
        if (stage.render_target != nullptr)
        {
            auto* render_target{ dynamic_cast<const dx12_render_target_t*>(stage.render_target) };
            auto* color_texture{ render_target ? render_target->dx12_color_texture() : nullptr };
            if (!render_target || !color_texture)
            {
                LOG_GRAPHICS_FATAL("DX12 offscreen textured quad stage requires a DX12 render target");
                return;
            }

            ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
            if (!color_texture->transition_to(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET))
                return;

            const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ render_target->rtv_handle() };
            cmd->OMSetRenderTargets(1u, &rtv, FALSE, nullptr);
            if (stage.target_load_action == quad_stage_common_t::target_load_action_t::clear)
            {
                float clear[]{
                    stage.target_clear_color.x,
                    stage.target_clear_color.y,
                    stage.target_clear_color.z,
                    stage.target_clear_color.w
                };
                cmd->ClearRenderTargetView(rtv, clear, 0u, nullptr);
            }

            record_quad_stage_to_active_target(recorded_stage.direct_stage,
                                               recorded_stage.stage_slot,
                                               recorded_stage.pipeline_kind);

            if (!color_texture->transition_to(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
                return;

            const dx12_swapchain_t* sc{ _swapchain.get() };
            if (sc)
            {
                const D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_rtv{ sc->get_current_rtv(_rtv_descriptor_stride) };
                cmd->OMSetRenderTargets(1u, &backbuffer_rtv, FALSE, nullptr);
            }
        }
        else if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
        {
            if (stage.capture_presentation_before_draw)
            {
                const dx12_swapchain_t* sc{ _swapchain.get() };
                record_capture_textured_quad_stage_to_active_target(recorded_stage.direct_stage,
                                                                    recorded_stage.stage_slot,
                                                                    recorded_stage.pipeline_kind,
                                                                    sc->get_backbuffer(sc->get_current_image_index()),
                                                                    sc->get_current_rtv(_rtv_descriptor_stride));
            }
            else
            {
                record_quad_stage_to_active_target(recorded_stage.direct_stage,
                                                   recorded_stage.stage_slot,
                                                   recorded_stage.pipeline_kind);
            }
        }
    }

    void dx12_rhi_context_t::record_text_quad_stage(const textured_quad_stage_record_t& stage)
    {
        const uint32_t stage_slot{ static_cast<uint32_t>(_recorded_quad_stages.size()) };
        _recorded_quad_stages.push_back({
            .kind = recorded_quad_stage_kind_t::direct,
            .stage_slot = stage_slot,
            .direct_stage = stage,
            .indirect_stage = { },
            .pipeline_kind = quad_pipeline_kind_t::text
        });
        recorded_quad_stage_t& recorded_stage{ _recorded_quad_stages.back() };
        recorded_stage.owned_direct_batches.assign(stage.batches.begin(), stage.batches.end());
        recorded_stage.direct_stage.batches = recorded_stage.owned_direct_batches;
        if (_recorded_quad_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_quad_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_quad_stages.pop_back();
            return;
        }
        if (stage.render_target != nullptr)
        {
            auto* render_target{ dynamic_cast<const dx12_render_target_t*>(stage.render_target) };
            auto* color_texture{ render_target ? render_target->dx12_color_texture() : nullptr };
            if (!render_target || !color_texture)
            {
                LOG_GRAPHICS_FATAL("DX12 offscreen text quad stage requires a DX12 render target");
                return;
            }

            ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
            if (!color_texture->transition_to(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET))
                return;

            const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ render_target->rtv_handle() };
            cmd->OMSetRenderTargets(1u, &rtv, FALSE, nullptr);
            if (stage.target_load_action == quad_stage_common_t::target_load_action_t::clear)
            {
                float clear[]{
                    stage.target_clear_color.x,
                    stage.target_clear_color.y,
                    stage.target_clear_color.z,
                    stage.target_clear_color.w
                };
                cmd->ClearRenderTargetView(rtv, clear, 0u, nullptr);
            }

            record_quad_stage_to_active_target(recorded_stage.direct_stage,
                                               recorded_stage.stage_slot,
                                               quad_pipeline_kind_t::text);

            if (!color_texture->transition_to(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
                return;

            const dx12_swapchain_t* sc{ _swapchain.get() };
            if (sc)
            {
                const D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_rtv{ sc->get_current_rtv(_rtv_descriptor_stride) };
                cmd->OMSetRenderTargets(1u, &backbuffer_rtv, FALSE, nullptr);
            }
        }
        else if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
        {
            record_quad_stage_to_active_target(recorded_stage.direct_stage,
                                               recorded_stage.stage_slot,
                                               quad_pipeline_kind_t::text);
        }
    }

    void dx12_rhi_context_t::record_indirect_textured_quad_stage(const indirect_textured_quad_stage_record_t& stage)
    {
        (void)stage;
        LOG_GRAPHICS_FATAL("DX12 indexed-indirect textured quad stages are unsupported after the instanced-only cleanup");
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

            for (const recorded_quad_stage_t& recorded_stage : _recorded_quad_stages)
            {
                if (recorded_stage.kind == recorded_quad_stage_kind_t::direct)
                {
                    const textured_quad_stage_record_t& stage{ recorded_stage.direct_stage };
                    if (!presentation_mask_includes(stage.presentation_mask, surface.presentation_channel_mask))
                        continue;
                    if (stage.render_target != nullptr)
                        continue;
                    if (stage.capture_presentation_before_draw)
                    {
                        record_capture_textured_quad_stage_to_active_target(stage,
                                                                            recorded_stage.stage_slot,
                                                                            recorded_stage.pipeline_kind,
                                                                            aux_backbuffer,
                                                                            aux_rtv);
                    }
                    else
                    {
                        record_quad_stage_to_active_target(stage, recorded_stage.stage_slot, recorded_stage.pipeline_kind);
                    }
                }
                else
                {
                    LOG_GRAPHICS_FATAL("DX12 auxiliary replay encountered an unsupported indexed-indirect stage");
                    break;
                }
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

        const bool has_initial_data{
            info.initial_data != nullptr && info.initial_data_size > 0u && info.initial_data_stride_bytes > 0u
        };

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
            has_initial_data ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&texture)
        ));
        DX12_NAME(texture, L"DX12 Texture2D");

        if (!has_initial_data)
        {
            auto result = std::make_unique<dx12_texture_t>(info.width,
                                                           info.height,
                                                           info.format,
                                                           texture,
                                                           resource_format,
                                                           srv_format,
                                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            result->set_has_initial_data(false);
            return result;
        }

        UINT64 upload_buffer_size{ 0 };
        device->GetCopyableFootprints(&texture_desc, 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size);

        const auto upload_allocation{
            _upload_ring ? _upload_ring->allocate(static_cast<size_t>(upload_buffer_size),
                                                  D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)
                         : std::nullopt
        };
        if (!upload_allocation)
        {
            LOG_GRAPHICS_ERROR("DX12 upload ring could not allocate {} bytes for texture upload", upload_buffer_size);
            texture->Release();
            return nullptr;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{ };
        UINT num_rows{ 0 };
        UINT64 row_size_in_bytes{ 0 };
        UINT64 total_bytes{ 0 };
        device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &num_rows, &row_size_in_bytes, &total_bytes);

        const std::byte* src_bytes{ static_cast<const std::byte*>(info.initial_data) };
        footprint.Offset = upload_allocation->offset_bytes;
        std::byte* dst_bytes{ upload_allocation->mapped_ptr };

        for (UINT row{ 0 }; row < num_rows; ++row)
        {
            std::memcpy(dst_bytes + row * footprint.Footprint.RowPitch,
                        src_bytes + row * info.initial_data_stride_bytes,
                        info.initial_data_stride_bytes);
        }

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
        src.pResource = upload_allocation->resource;
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
        if (_upload_ring)
            _upload_ring->reset();

        cmd->Release();
        allocator->Release();

        auto result = std::make_unique<dx12_texture_t>(info.width,
                                                       info.height,
                                                       info.format,
                                                       texture,
                                                       resource_format,
                                                       srv_format,
                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        result->set_has_initial_data(true);
        return result;
    }

    std::unique_ptr<rhi_render_target_t> dx12_rhi_context_t::create_render_target_2d(
        const render_target_create_info_t& info)
    {
        ID3D12Device* device{ _device ? _device->id3d12_device() : nullptr };
        if (!device || info.width == 0u || info.height == 0u)
            return nullptr;

        const DXGI_FORMAT resource_format{ dx12_texture_resource_format(info.format) };
        const DXGI_FORMAT srv_format{ dx12_texture_srv_format(info.format) };
        const DXGI_FORMAT rtv_format{ dx12_texture_rtv_format(info.format) };

        D3D12_HEAP_PROPERTIES default_heap{ };
        default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        default_heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        default_heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        default_heap.CreationNodeMask = 1u;
        default_heap.VisibleNodeMask = 1u;

        D3D12_RESOURCE_DESC texture_desc{ };
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Alignment = 0u;
        texture_desc.Width = info.width;
        texture_desc.Height = info.height;
        texture_desc.DepthOrArraySize = 1u;
        texture_desc.MipLevels = 1u;
        texture_desc.Format = resource_format;
        texture_desc.SampleDesc.Count = 1u;
        texture_desc.SampleDesc.Quality = 0u;
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear_value{ };
        clear_value.Format = rtv_format;
        clear_value.Color[0] = 0.0f;
        clear_value.Color[1] = 0.0f;
        clear_value.Color[2] = 0.0f;
        clear_value.Color[3] = 0.0f;

        ID3D12Resource* texture{ nullptr };
        DX12_CHECK(device->CreateCommittedResource(&default_heap,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &texture_desc,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                   &clear_value,
                                                   IID_PPV_ARGS(&texture)));
        DX12_NAME(texture, L"DX12 Render Target Texture");

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{ };
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.NumDescriptors = 1u;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtv_heap_desc.NodeMask = 0u;

        ID3D12DescriptorHeap* rtv_heap{ nullptr };
        DX12_CHECK(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)));
        DX12_NAME(rtv_heap, L"DX12 Offscreen RTV Heap");

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ rtv_heap->GetCPUDescriptorHandleForHeapStart() };
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{ };
        rtv_desc.Format = rtv_format;
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0u;
        rtv_desc.Texture2D.PlaneSlice = 0u;
        device->CreateRenderTargetView(texture, &rtv_desc, rtv_handle);

        auto color_texture = std::make_unique<dx12_texture_t>(info.width,
                                                              info.height,
                                                              info.format,
                                                              texture,
                                                              resource_format,
                                                              srv_format,
                                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        return std::make_unique<dx12_render_target_t>(std::move(color_texture), rtv_heap, rtv_handle);
    }

    std::unique_ptr<rhi_buffer_t> dx12_rhi_context_t::create_buffer(const buffer_create_info_t& info)
    {
        ID3D12Device* device{ _device ? _device->id3d12_device() : nullptr };
        if (!device)
        {
            LOG_GRAPHICS_ERROR("DX12 create_buffer called without a valid device");
            return nullptr;
        }

        const bool cpu_visible{
            info.cpu_writable || buffer_usage_prefers_upload_memory(info.usage) ||
            buffer_usage_prefers_readback_memory(info.usage)
        };

        if (cpu_visible || !info.initial_data)
            return std::make_unique<dx12_buffer_t>(device, info);

        auto gpu_buffer{ std::make_unique<dx12_buffer_t>(device, buffer_create_info_t{
            .size_bytes = info.size_bytes,
            .usage = info.usage,
            .initial_data = nullptr,
            .cpu_writable = info.cpu_writable
        }) };

        const auto upload_allocation{
            _upload_ring ? _upload_ring->allocate(info.size_bytes, 16u) : std::nullopt
        };
        if (!upload_allocation)
        {
            LOG_GRAPHICS_ERROR("DX12 upload ring could not allocate {} bytes for buffer upload", info.size_bytes);
            return nullptr;
        }
        std::memcpy(upload_allocation->mapped_ptr, info.initial_data, info.size_bytes);

        ID3D12CommandAllocator* allocator{ nullptr };
        ID3D12GraphicsCommandList* cmd{ nullptr };

        DX12_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        DX12_CHECK(device->CreateCommandList(0,
                                             D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             allocator,
                                             nullptr,
                                             IID_PPV_ARGS(&cmd)));

        DX12_NAME(allocator, L"DX12 Buffer Upload Command Allocator");
        DX12_NAME(cmd, L"DX12 Buffer Upload Command List");

        D3D12_RESOURCE_BARRIER to_copy_dest{ };
        to_copy_dest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_copy_dest.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_copy_dest.Transition.pResource = gpu_buffer->resource();
        to_copy_dest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_copy_dest.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        to_copy_dest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        cmd->ResourceBarrier(1, &to_copy_dest);

        cmd->CopyBufferRegion(gpu_buffer->resource(),
                              0u,
                              upload_allocation->resource,
                              upload_allocation->offset_bytes,
                              info.size_bytes);

        D3D12_RESOURCE_BARRIER to_common{ };
        to_common.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_common.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_common.Transition.pResource = gpu_buffer->resource();
        to_common.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_common.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        to_common.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        cmd->ResourceBarrier(1, &to_common);

        DX12_CHECK(cmd->Close());

        ID3D12CommandList* command_lists[]{ cmd };
        _graphics_queue->id3d12_command_queue()->ExecuteCommandLists(1, command_lists);

        auto upload_fence{ std::make_unique<dx12_fence_t>(device) };
        upload_fence->signal(_graphics_queue->id3d12_command_queue());
        upload_fence->wait();
        if (_upload_ring)
            _upload_ring->reset();

        cmd->Release();
        allocator->Release();

        return gpu_buffer;
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
        return std::make_unique<dx12_sampler_t>(_device->id3d12_device(), desc);
    }

    rhi_sampler_t* dx12_rhi_context_t::get_or_create_sampler(const sampler_desc_t& desc)
    {
        if (const auto it = _sampler_cache.find(desc); it != _sampler_cache.end())
            return it->second.get();

        auto sampler = std::make_unique<dx12_sampler_t>(_device->id3d12_device(), desc);
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

        if (!_recorded_quad_stages.empty())
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

        dx12_frame_t& frame{ _frames[_frame_index] };
        ID3D12GraphicsCommandList* cmd{ frame.command_list->id3d12_graphics_command_list() };
        if (!cmd)
            return;

        const auto* default_buffer{ dynamic_cast<const dx12_buffer_t*>(_default_compute_storage_buffer.get()) };
        if (!default_buffer)
        {
            LOG_GRAPHICS_ERROR("DX12 default compute storage buffer is not available");
            return;
        }

        cmd->SetComputeRootSignature(pipeline->root_signature());
        cmd->SetPipelineState(pipeline->pipeline_state());

        for (std::uint32_t slot{ 0u }; slot < k_max_compute_buffer_bindings; ++slot)
        {
            const rhi_buffer_t* bound_buffer{ _default_compute_storage_buffer.get() };
            for (const compute_buffer_binding_t& binding : record.read_only_buffers)
            {
                if (binding.slot == slot && binding.buffer)
                {
                    bound_buffer = binding.buffer;
                    break;
                }
            }

            const dx12_buffer_t* dx_buffer{ dynamic_cast<const dx12_buffer_t*>(bound_buffer) };
            if (!dx_buffer)
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch received invalid read-only buffer");
                return;
            }

            if (!dx_buffer->flush_pending_upload(cmd))
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch failed to flush pending read-only-buffer upload");
                return;
            }

            if (!dx_buffer->transition_to(cmd, D3D12_RESOURCE_STATE_GENERIC_READ))
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch failed to transition read-only buffer to generic-read state");
                return;
            }
            cmd->SetComputeRootShaderResourceView(slot, dx_buffer->resource()->GetGPUVirtualAddress());

            bound_buffer = _default_compute_storage_buffer.get();
            for (const compute_buffer_binding_t& binding : record.storage_buffers)
            {
                if (binding.slot == slot && binding.buffer)
                {
                    bound_buffer = binding.buffer;
                    break;
                }
            }

            dx_buffer = dynamic_cast<const dx12_buffer_t*>(bound_buffer);
            if (!dx_buffer)
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch received invalid storage buffer");
                return;
            }

            if (!dx_buffer->flush_pending_upload(cmd))
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch failed to flush pending storage-buffer upload");
                return;
            }
            if (!dx_buffer->transition_to(cmd, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_GRAPHICS_ERROR("DX12 compute dispatch failed to transition storage buffer to UAV state");
                return;
            }
            cmd->SetComputeRootUnorderedAccessView(k_max_compute_buffer_bindings + slot,
                                                   dx_buffer->resource()->GetGPUVirtualAddress());
        }

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

            cmd->SetComputeRootConstantBufferView(k_max_compute_buffer_bindings * 2u,
                                                 constant_buffer->resource()->GetGPUVirtualAddress());
            frame.transient_compute_constant_buffers.push_back(std::move(constant_buffer));
        }

        cmd->Dispatch(record.group_count_x, record.group_count_y, record.group_count_z);

        if (record.graphics_handoff == compute_graphics_handoff_t::storage_write_to_graphics_read)
        {
            D3D12_RESOURCE_BARRIER uav_barrier{ };
            uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            std::vector<const dx12_buffer_t*> buffers_to_transition;
            buffers_to_transition.reserve(record.storage_buffers.size());
            std::vector<D3D12_RESOURCE_BARRIER> uav_barriers;
            uav_barriers.reserve(record.storage_buffers.size());

            for (const compute_buffer_binding_t& binding : record.storage_buffers)
            {
                if (!binding.buffer)
                    continue;

                const auto* dx_buffer{ dynamic_cast<const dx12_buffer_t*>(binding.buffer) };
                if (!dx_buffer)
                    continue;

                D3D12_RESOURCE_BARRIER compute_uav_barrier{ uav_barrier };
                compute_uav_barrier.UAV.pResource = dx_buffer->resource();
                uav_barriers.push_back(compute_uav_barrier);
                buffers_to_transition.push_back(dx_buffer);
            }

            if (!uav_barriers.empty())
                cmd->ResourceBarrier(static_cast<UINT>(uav_barriers.size()), uav_barriers.data());

            for (const dx12_buffer_t* dx_buffer : buffers_to_transition)
            {
                if (!dx_buffer->transition_to(cmd, D3D12_RESOURCE_STATE_GENERIC_READ))
                {
                    LOG_GRAPHICS_ERROR("DX12 compute dispatch failed to transition storage buffer to generic-read state");
                    return;
                }
            }
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

    dx12_textured_quad_pipeline_t* dx12_rhi_context_t::resolve_quad_pipeline(
        const quad_pipeline_kind_t pipeline_kind) const noexcept
    {
        switch (pipeline_kind)
        {
            case quad_pipeline_kind_t::text: return _instanced_text_quad_pipeline.get();
            case quad_pipeline_kind_t::battle_swirl: return _instanced_battle_swirl_pipeline.get();
            case quad_pipeline_kind_t::bloom_blur: return _instanced_bloom_blur_pipeline.get();
            case quad_pipeline_kind_t::bloom_composite: return _instanced_bloom_composite_pipeline.get();
            case quad_pipeline_kind_t::textured:
            default: return _instanced_textured_quad_pipeline.get();
        }
    }

    void dx12_rhi_context_t::ensure_textured_quad_descriptor_capacity(const uint32_t stage_slot,
                                                                      const uint32_t required_capacity)
    {
        if (required_capacity == 0 || stage_slot >= k_max_textured_quad_stage_slots_per_frame)
            return;

        for (uint32_t frame_index{ 0 }; frame_index < k_max_frames_in_flight; ++frame_index)
        {
            dx12_frame_t& frame{ _frames[frame_index] };
            const uint32_t current_capacity{ frame.textured_quad_descriptor_capacities[stage_slot] };
            const bool has_srv_heap{ frame.textured_quad_srv_heaps[stage_slot] != nullptr };
            const bool has_sampler_heap{ frame.textured_quad_sampler_heaps[stage_slot] != nullptr };
            if (current_capacity >= required_capacity && has_srv_heap && has_sampler_heap)
            {
                continue;
            }

            const uint32_t target_capacity{
                std::max(std::bit_ceil(required_capacity), 128u)
            };
            // Direct textured/text quad stages bind 1 CBV plus 5 SRVs per batch:
            // forward+ light input, forward+ output, world items, visible item indices, texture.
            const uint32_t srv_heap_descriptor_count{ (target_capacity * 5u) + 1u };

            frame.fence->wait(frame.fence_value);

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

            frame.textured_quad_descriptor_capacities[stage_slot] = target_capacity;
        }
    }

    void dx12_rhi_context_t::ensure_compute_descriptor_capacity(const uint32_t required_capacity)
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        if (required_capacity == 0u)
            return;

        if (frame.compute_uav_heap && frame.compute_descriptor_capacity >= required_capacity)
            return;

        const uint32_t target_capacity{
            std::max(required_capacity, k_max_textured_quad_stage_slots_per_frame * (k_max_compute_buffer_bindings * 2u))
        };
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = target_capacity;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (frame.compute_uav_heap)
        {
            LOG_GRAPHICS_FATAL("DX12 compute UAV heap capacity was exhausted mid-frame");
            return;
        }

        DX12_CHECK(_device->id3d12_device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&frame.compute_uav_heap)));
        DX12_NAME(frame.compute_uav_heap, L"DX12 Compute UAV Heap");
        frame.compute_descriptor_capacity = target_capacity;
    }

} // namespace carrot::rhi::dx12
