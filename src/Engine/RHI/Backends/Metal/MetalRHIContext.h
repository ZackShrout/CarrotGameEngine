//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "MetalRenderEncoder.h"
#include "RHI/RHI.h"

#include <memory>
#include <span>
#include <vector>

namespace carrot::rhi::metal {
    class metal_swapchain_t;
    class metal_command_queue_t;
    class metal_device_t;
    class metal_buffer_t;
    class metal_texture_t;
    class metal_textured_quad_pipeline_t;

    class metal_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit metal_rhi_context_t(const rhi_desc_t& desc);
        ~metal_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void release_asset_references() override {}

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;

        void set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer,
                                        const rhi_buffer_t& index_buffer) override;

        void set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches) override;

        void wait_idle() override;

    private:
        [[nodiscard]] bool is_frame_active() const noexcept;
        void reset_frame_state() noexcept;

        [[nodiscard]] MTL::SamplerState* get_textured_quad_sampler(renderer::sampler_filter_t filter) const noexcept;
        void ensure_textured_quad_argument_capacity(size_t batch_count);
        void encode_textured_quad_argument_buffers(const metal_texture_t& texture, size_t batch_index,
                                                   size_t& out_root_ab_offset) const;


        [[nodiscard]] static size_t align_up(size_t value, size_t alignment = 8) noexcept;

        std::unique_ptr<metal_device_t>                 _device;
        std::unique_ptr<metal_swapchain_t>              _swapchain;
        std::unique_ptr<metal_command_queue_t>          _command_queue;
        std::unique_ptr<metal_textured_quad_pipeline_t> _textured_quad_pipeline;

        metal_render_encoder_t                          _render_encoder;
        void*                                           _metal_layer{ nullptr };

        dispatch_semaphore_t                            _frame_semaphore{ nullptr };
        // MTL::SamplerState*                              _textured_quad_sampler{ nullptr };
        MTL::SamplerState*                              _textured_quad_sampler_nearest{ nullptr };
        MTL::SamplerState*                              _textured_quad_sampler_linear{ nullptr };

        const metal_buffer_t*                           _textured_quad_vertex_buffer{ nullptr };
        const metal_buffer_t*                           _textured_quad_index_buffer{ nullptr };
        std::vector<renderer::textured_quad_batch_t>    _textured_quad_batches;

        // Dynamic per-batch explicit root-signature data.
        std::unique_ptr<metal_buffer_t>                 _textured_quad_root_argument_buffer;
        std::unique_ptr<metal_buffer_t>                 _textured_quad_srv_descriptor_table;
        std::unique_ptr<metal_buffer_t>                 _textured_quad_sampler_descriptor_table;

        size_t                                          _textured_quad_root_stride{ 0 };
        size_t                                          _textured_quad_srv_stride{ 0 };
        size_t                                          _textured_quad_sampler_stride{ 0 };
        size_t                                          _textured_quad_argument_capacity{ 0 };

        MTL::CommandBuffer*                             _active_command_buffer{ nullptr };
        const CA::MetalDrawable*                        _active_drawable{ nullptr };
    };
} // namespace carrot::rhi::metal
