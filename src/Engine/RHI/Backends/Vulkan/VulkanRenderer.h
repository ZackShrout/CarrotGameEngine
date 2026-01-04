//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Renderer/Renderer.h"
#include "VulkanCommon.h"
#include "VulkanCore.h"

#include <vector>

namespace carrot::rhi::vulkan {
    class vulkan_context_t;

    class vulkan_renderer_t : public renderer::renderer_t
    {
    public:
        void init() override;
        void shutdown() override;

        void begin_frame() override;
        void render_frame() override; // temporary triangle, will be replaced later
        void end_frame() override;

        void reload_pipeline() override;

        [[nodiscard]] VkCommandBuffer get_current_command_buffer() const noexcept
        {
            return _frames[_current_frame].command_buffer;
        }

    private:
        void create_pipeline();
        void destroy_pipeline();
        void recreate_swapchain_dependent_resources();

        vulkan_context_t* _ctx{ nullptr };

        pipeline_t _graphics_pipeline;
        pipeline_layout_t _pipeline_layout;
        render_pass_t _render_pass;
        command_pool_t _command_pool;

        framebuffer_array_t _swapchain_framebuffers;
        frame_data_t _frames;

        uint32_t _current_frame{ 0 };
        uint32_t _frame_counter{ 0 };
        uint32_t _current_image_index{ 0 };
    };
} // namespace carrot::rhi::vulkan
