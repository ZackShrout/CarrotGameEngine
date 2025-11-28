#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace carrot::rhi {
    struct vulkan_context_t;
}

namespace carrot {

class vulkan_renderer_t
{
public:
    static void init();
    static void shutdown();

    static void begin_frame();
    static void render_frame();   // temporary triangle, will be replaced later
    static void end_frame();

    static void reload_pipeline();  // for hot-reload later today

private:
    static void create_pipeline();
    static void destroy_pipeline();
    static void recreate_swapchain_dependent_resources();

private:
    // Core context (lives for the whole application)
    static rhi::vulkan_context_t* _ctx;

    // Pipeline
    static VkPipeline             _graphics_pipeline;
    static VkPipelineLayout       _pipeline_layout;
    static VkRenderPass           _render_pass;

    // Swapchain-dependent
    static std::vector<VkFramebuffer>       _swapchain_framebuffers;

    // Per-frame resources
    static VkCommandPool                     _command_pool;
    static std::vector<VkCommandBuffer>      _command_buffers;

    static std::vector<VkSemaphore>          _image_available_semaphores;
    static std::vector<VkSemaphore>          _render_finished_semaphores;
    static std::vector<VkFence>              _in_flight_fences;

    static uint32_t _current_frame;
    static uint32_t _frame_counter;
    static uint32_t _current_image_index;
};

} // namespace carrot