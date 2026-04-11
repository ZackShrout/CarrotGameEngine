//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Buffer.h"
#include "DirectX12Common.h"
#include "DirectX12Core.h"
#include "DirectX12Sampler.h"
#include "DirectX12Texture.h"
#include "Pipelines/DirectX12TexturedQuadPipeline.h"
#include "RHI/RHI.h"
#include "Renderer/Draw/TexturedQuadCameraUniform.h"

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace carrot::rhi::dx12 {
    constexpr uint32_t k_max_textured_quad_stage_records_per_frame{ 8 };

    class dx12_device_t;
    class dx12_command_queue_t;
    class dx12_swapchain_t;
    class dx12_fence_t;
    class dx12_command_list_t;

    struct dx12_frame_t
    {
        ID3D12CommandAllocator* allocator{ nullptr };
        std::unique_ptr<dx12_command_list_t> command_list;
        std::unique_ptr<dx12_fence_t> fence;
        uint64_t fence_value{ 0 };
        std::array<std::unique_ptr<dx12_buffer_t>, k_max_textured_quad_stage_records_per_frame>
            textured_quad_camera_uniform_buffers;

        std::array<ID3D12DescriptorHeap*, k_max_textured_quad_stage_records_per_frame> textured_quad_srv_heaps{ };
        std::array<ID3D12DescriptorHeap*, k_max_textured_quad_stage_records_per_frame> textured_quad_sampler_heaps{ };
        uint32_t textured_quad_descriptor_capacity{ 0 };
    };

    class dx12_rhi_context_t final : public rhi_context_t, public dx12_textured_quad_sampler_provider_t
    {
    public:
        explicit dx12_rhi_context_t(const rhi_desc_t& desc);
        ~dx12_rhi_context_t() override;

        void begin_frame() override;
        void record_textured_quad_stage(const textured_quad_stage_record_t& stage) override;
        void record_text_quad_stage(const textured_quad_stage_record_t& stage) override;
        void end_frame() override;

        void release_asset_references() override {}

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;
        [[nodiscard]] graphics_api get_graphics_api() const noexcept override { return graphics_api::direct_x12; }

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) override;
        bool add_presentation_window(window::window_id_t window_id,
                                     uint32_t presentation_channel_mask = presentation_channel_gameplay) override;
        bool remove_presentation_window(window::window_id_t window_id) override;

        void wait_idle() override;

    private:
        enum class quad_pipeline_kind_t : uint8_t
        {
            textured = 0,
            text
        };

        struct auxiliary_surface_t
        {
            window::window_id_t id{ window::invalid_window_id };
            uint32_t presentation_channel_mask{ presentation_channel_gameplay };
            std::unique_ptr<dx12_swapchain_t> swapchain;
            uint32_t last_width{ 0 };
            uint32_t last_height{ 0 };
        };

        struct recorded_stage_t
        {
            textured_quad_stage_record_t stage;
            quad_pipeline_kind_t pipeline_kind{ quad_pipeline_kind_t::textured };
        };

        void record_quad_stage_to_active_target(const textured_quad_stage_record_t& stage,
                                                quad_pipeline_kind_t pipeline_kind);
        void sync_auxiliary_surface_sizes();
        bool create_auxiliary_surface(window::window_id_t window_id, uint32_t presentation_channel_mask);
        void destroy_auxiliary_surface(auxiliary_surface_t& surface) noexcept;

        void ensure_textured_quad_descriptor_capacity(uint32_t required_capacity);

        // ── Backend-owned services and persistent objects ──
        std::unique_ptr<dx12_device_t>                    _device;
        std::unique_ptr<dx12_command_queue_t>             _graphics_queue;
        std::unique_ptr<dx12_swapchain_t>                 _swapchain;
        std::unique_ptr<dx12_textured_quad_pipeline_t>    _textured_quad_pipeline;
        std::unique_ptr<dx12_textured_quad_pipeline_t>    _text_quad_pipeline;

        // ── Per-frame GPU resources and frame progression ──
        std::array<dx12_frame_t, k_max_frames_in_flight>  _frames;
        uint32_t                                          _frame_index{ 0 };
        window::window_id_t                               _presentation_window_id{ window::invalid_window_id };
        std::vector<auxiliary_surface_t>                  _auxiliary_surfaces;
        std::vector<recorded_stage_t>                     _recorded_stages;

        // ── Swapchain / render-target descriptor bookkeeping ──
        uint32_t                                          _rtv_descriptor_stride{ 0 };

        // ── Dynamic per-batch descriptor state ──
        uint32_t                                          _srv_descriptor_stride{ 0 };
        uint32_t                                          _sampler_descriptor_stride{ 0 };

        // ── Sampler caching ──
        std::unordered_map<sampler_desc_t, std::unique_ptr<dx12_sampler_t>, sampler_desc_hash_t> _sampler_cache;
    };
} // namespace carrot::rhi::dx12
