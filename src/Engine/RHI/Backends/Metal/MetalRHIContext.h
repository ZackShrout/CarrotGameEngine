//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "MetalRenderEncoder.h"
#include "RHI/RHI.h"
#include "Window/Window.h"

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace carrot::rhi::metal {
    namespace assets = carrot::assets;
    class metal_swapchain_t;
    class metal_command_queue_t;
    class metal_device_t;
    class metal_buffer_t;
    class metal_texture_t;
    class metal_compute_pipeline_t;
    class metal_textured_quad_pipeline_t;

    class metal_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit metal_rhi_context_t(const rhi_desc_t& desc);
        ~metal_rhi_context_t() override;

        void begin_frame() override;
        void record_textured_quad_stage(const textured_quad_stage_record_t& stage) override;
        void record_indirect_textured_quad_stage(const indirect_textured_quad_stage_record_t& stage) override;
        void record_text_quad_stage(const textured_quad_stage_record_t& stage) override;
        void end_frame() override;

        void release_asset_references() override {}

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;
        [[nodiscard]] graphics_api get_graphics_api() const noexcept override { return graphics_api::metal; }

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_compute_pipeline_t> create_compute_pipeline(
            const compute_pipeline_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources([[maybe_unused]] const rhi_texture_t& texture,
                                          [[maybe_unused]] const rhi_sampler_t& sampler) override {}
        void dispatch_compute(const compute_dispatch_record_t& record) override;
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
            std::unique_ptr<metal_swapchain_t> swapchain;
            const CA::MetalDrawable* drawable{ nullptr };
        };

        struct recorded_stage_t
        {
            textured_quad_stage_record_t stage;
            uint32_t stage_slot{ 0 };
            quad_pipeline_kind_t pipeline_kind{ quad_pipeline_kind_t::textured };
        };

        struct recorded_indirect_stage_t
        {
            indirect_textured_quad_stage_record_t stage;
            uint32_t stage_slot{ 0 };
        };

        [[nodiscard]] bool is_frame_active() const noexcept;
        void begin_main_render_encoder_if_needed();
        void reset_frame_state() noexcept;
        void acquire_auxiliary_drawables();
        void release_auxiliary_drawables() noexcept;
        void encode_quad_stage(MTL::RenderCommandEncoder* encoder,
                               const textured_quad_stage_record_t& stage,
                               chlm::uint2 target_size_px,
                               uint32_t stage_slot,
                               quad_pipeline_kind_t pipeline_kind);
        void encode_indirect_textured_quad_stage(MTL::RenderCommandEncoder* encoder,
                                                 const indirect_textured_quad_stage_record_t& stage,
                                                 chlm::uint2 target_size_px,
                                                 uint32_t stage_slot);

        void ensure_textured_quad_argument_capacity(uint32_t stage_slot, size_t batch_count);
        void encode_textured_quad_argument_buffers(const metal_texture_t& texture,
                                                   const rhi_buffer_t* forward_plus_light_input_buffer,
                                                   const rhi_buffer_t* forward_plus_output_buffer,
                                                   const rhi_buffer_t* world_item_buffer,
                                                   const rhi_buffer_t* visible_item_index_buffer,
                                                   uint32_t stage_slot,
                                                   size_t batch_index,
                                                   const renderer::textured_quad_batch_t& batch,
                                                   size_t& out_root_ab_offset);

        [[nodiscard]] static size_t align_up(size_t value, size_t alignment = 8) noexcept;

        // ── Core Metal handles & synchronization ──
        void*                                           _metal_layer{ nullptr };
        window::window_id_t                             _presentation_window_id{ window::invalid_window_id };
        dispatch_semaphore_t                            _frame_semaphore{ nullptr };

        // ── Backend-owned services and persistent objects ──
        assets::shader_file_provider_t*              _shader_files{ nullptr };
        std::unique_ptr<metal_device_t>                 _device;
        std::unique_ptr<metal_swapchain_t>              _swapchain;
        std::unique_ptr<metal_command_queue_t>          _command_queue;
        std::unique_ptr<metal_textured_quad_pipeline_t> _textured_quad_pipeline;
        std::unique_ptr<metal_textured_quad_pipeline_t> _text_quad_pipeline;
        std::unique_ptr<rhi_buffer_t>                   _default_compute_storage_buffer;

        // ── Per-frame / active submission state ──
        metal_render_encoder_t                          _render_encoder;
        MTL::CommandBuffer*                             _active_command_buffer{ nullptr };
        const CA::MetalDrawable*                        _active_drawable{ nullptr };
        std::vector<auxiliary_surface_t>                _auxiliary_surfaces;
        std::vector<recorded_stage_t>                   _recorded_stages;
        std::vector<recorded_indirect_stage_t>          _recorded_indirect_stages;

        std::array<std::unique_ptr<metal_buffer_t>, k_max_textured_quad_stage_slots_per_frame>
                                                      _textured_quad_camera_uniform_buffers;

        // ── Dynamic per-batch argument / root signature data ──
        std::array<std::unique_ptr<metal_buffer_t>, k_max_textured_quad_stage_slots_per_frame>
                                                      _textured_quad_root_argument_buffers;
        std::array<std::unique_ptr<metal_buffer_t>, k_max_textured_quad_stage_slots_per_frame>
                                                      _textured_quad_cbv_descriptor_tables;
        std::array<std::unique_ptr<metal_buffer_t>, k_max_textured_quad_stage_slots_per_frame>
                                                      _textured_quad_srv_descriptor_tables;
        std::array<std::unique_ptr<metal_buffer_t>, k_max_textured_quad_stage_slots_per_frame>
                                                      _textured_quad_sampler_descriptor_tables;
        std::vector<std::unique_ptr<metal_buffer_t>> _transient_compute_buffers;

        size_t                                          _textured_quad_root_stride{ 0 };
        size_t                                          _textured_quad_cbv_stride{ 0 };
        size_t                                          _textured_quad_srv_stride{ 0 };
        size_t                                          _textured_quad_sampler_stride{ 0 };
        std::array<size_t, k_max_textured_quad_stage_slots_per_frame> _textured_quad_argument_capacities{ };

        // ── Sampler caching ──
        std::unordered_map<sampler_desc_t, std::unique_ptr<rhi_sampler_t>, sampler_desc_hash_t> _sampler_cache;
    };
} // namespace carrot::rhi::metal
