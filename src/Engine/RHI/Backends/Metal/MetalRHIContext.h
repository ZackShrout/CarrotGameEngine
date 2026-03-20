//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "MetalCore.h"
#include "MetalRenderEncoder.h"
#include "RHI/RHI.h"

namespace carrot::rhi::metal {
    class metal_swapchain_t;
    class metal_command_queue_t;
    class metal_device_t;

    class metal_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit metal_rhi_context_t(const rhi_desc_t& desc);
        ~metal_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        void set_textured_quad_texture(const rhi_texture_t& texture) override {}
        void set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer, const rhi_buffer_t& index_buffer) override {}


        void wait_idle() override;

    private:
        std::unique_ptr<metal_device_t>         _device;
        std::unique_ptr<metal_swapchain_t>      _swapchain;
        std::unique_ptr<metal_command_queue_t>  _command_queue;
        render_pipeline_state_t                 _triangle_pipeline;
        metal_render_encoder_t                  _render_encoder;
        void*                                   _metal_layer{ nullptr }; // CAMetalLayer*

        static constexpr uint32_t               k_push_buffer_count{ 3 };
        MTL::Buffer*                            _push_buffers[k_push_buffer_count] { nullptr };
        uint32_t                                _current_push_index{ 0 };
        uint32_t                                _frame_counter{ 0 };

        dispatch_semaphore_t                    _frame_semaphore{ nullptr };
    };
} // namespace carrot::rhi::metal
