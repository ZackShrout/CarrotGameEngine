//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Renderer/Renderer.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace carrot::rhi {
    struct vulkan_context_t;
}

namespace carrot {
    class vulkan_renderer_t : public renderer::renderer_t
    {
    public:
        void init() override;
        void shutdown() override;

        void begin_frame() override;
        void render_frame() override; // temporary triangle, will be replaced later
        void end_frame() override;

        void reload_pipeline() override;

        static void render_debug_overlay() noexcept;

        [[nodiscard]] static VkCommandBuffer get_current_command_buffer() noexcept
        {
            return _command_buffers[_current_frame];
        }

    private:
        static void create_pipeline();
        static void destroy_pipeline();
        static void recreate_swapchain_dependent_resources();

        // Core context (lives for the whole application)
        static rhi::vulkan_context_t* _ctx;

        // Pipeline
        static VkPipeline _graphics_pipeline;
        static VkPipelineLayout _pipeline_layout;
        static VkRenderPass _render_pass;

        // Swapchain-dependent
        static std::vector<VkFramebuffer> _swapchain_framebuffers;

        // Per-frame resources
        static VkCommandPool _command_pool;
        static std::vector<VkCommandBuffer> _command_buffers;

        static std::vector<VkSemaphore> _image_available_semaphores;
        static std::vector<VkSemaphore> _render_finished_semaphores;
        static std::vector<VkFence> _in_flight_fences;

        static uint32_t _current_frame;
        static uint32_t _frame_counter;
        static uint32_t _current_image_index;
    };
} // namespace carrot

namespace carrot::renderer {
    renderer_t* create_backend();
} // namespace carrot::renderer
