#include <cstdint>
#include <cstdio>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "Engine/Core/Platform/Wayland/WaylandWindow.h"
#include "Engine/RHI/VulkanContext.h"

static std::vector<uint32_t> load_spv(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Failed to open %s\n", path); return {}; }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint32_t> data((sz + 3) / 4);
    fread(data.data(), 1, sz, f);
    fclose(f);
    return data;
}

int main()
{
    constexpr uint32_t k_width{ 1280 };
    constexpr uint32_t k_height{ 720 };
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;  // ← Most GPUs support at least 2

    carrot::platform::wayland_window window(k_width, k_height, "Carrot Engine – Month 0 Complete");

    VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Carrot", 1, "Carrot", 1, VK_API_VERSION_1_3 };
    const char* exts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
    VkInstanceCreateInfo inst_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &app_info, 0, nullptr, 2, exts };
    VkInstance instance; vkCreateInstance(&inst_info, nullptr, &instance);

    VkWaylandSurfaceCreateInfoKHR surf_info{ VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    surf_info.display = window.get_wl_display();
    surf_info.surface = window.get_wl_surface();
    VkSurfaceKHR surface; vkCreateWaylandSurfaceKHR(instance, &surf_info, nullptr, &surface);

    carrot::rhi::vulkan_context ctx;
    ctx.init(instance, surface);
    ctx.create_swapchain(k_width, k_height);

    auto vert = load_spv("bin/debug/shaders/triangle.spv");
    auto frag = load_spv("bin/debug/shaders/triangle.frag.spv");
    if (vert.empty() || frag.empty()) return 1;

    VkShaderModule vert_mod, frag_mod;
    VkShaderModuleCreateInfo mod_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    mod_info.codeSize = vert.size() * 4; mod_info.pCode = vert.data();
    vkCreateShaderModule(ctx.device, &mod_info, nullptr, &vert_mod);
    mod_info.codeSize = frag.size() * 4; mod_info.pCode = frag.data();
    vkCreateShaderModule(ctx.device, &mod_info, nullptr, &frag_mod);

    VkPushConstantRange push{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(ctx.device, &layout_info, nullptr, &pipeline_layout);

    VkAttachmentDescription color_att{};
    color_att.format = VK_FORMAT_B8G8R8A8_SRGB;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass{ 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &color_ref };
    VkRenderPassCreateInfo rp_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, &color_att, 1, &subpass };
    VkRenderPass render_pass;
    vkCreateRenderPass(ctx.device, &rp_info, nullptr, &render_pass);

    VkPipelineShaderStageCreateInfo stages[2]{
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert_mod, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag_mod, "main", nullptr }
    };

    VkPipelineVertexInputStateCreateInfo empty_vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr };
    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT };
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &blend };
    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyn_states };

    VkGraphicsPipelineCreateInfo pipe_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipe_info.stageCount = 2; pipe_info.pStages = stages;
    pipe_info.pVertexInputState = &empty_vertex_input;
    pipe_info.pInputAssemblyState = &ia;
    pipe_info.pViewportState = &vp;
    pipe_info.pRasterizationState = &rs;
    pipe_info.pMultisampleState = &ms;
    pipe_info.pColorBlendState = &cb;
    pipe_info.pDynamicState = &dyn;
    pipe_info.layout = pipeline_layout;
    pipe_info.renderPass = render_pass;

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &pipeline);

    std::vector<VkFramebuffer> fbs(ctx.image_count);
    for (uint32_t i = 0; i < ctx.image_count; ++i) {
        VkFramebufferCreateInfo fb_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, render_pass, 1, &ctx.swapchain_views[i], ctx.swapchain_extent.width, ctx.swapchain_extent.height, 1 };
        vkCreateFramebuffer(ctx.device, &fb_info, nullptr, &fbs[i]);
    }

    VkCommandPool pool;
    VkCommandPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, ctx.graphics_family };
    vkCreateCommandPool(ctx.device, &pool_info, nullptr, &pool);

    std::vector<VkCommandBuffer> cmds(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, MAX_FRAMES_IN_FLIGHT };
    vkAllocateCommandBuffers(ctx.device, &alloc_info, cmds.data());

    std::vector<VkSemaphore> image_avail(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> render_done(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> in_flight(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo sem_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(ctx.device, &sem_info, nullptr, &image_avail[i]);
        vkCreateSemaphore(ctx.device, &sem_info, nullptr, &render_done[i]);
        vkCreateFence(ctx.device, &fence_info, nullptr, &in_flight[i]);
    }

    uint32_t frame_counter = 0;
    uint32_t current_frame = 0;

    while (!window.should_close()) {
        window.poll_events();

        vkWaitForFences(ctx.device, 1, &in_flight[current_frame], VK_TRUE, ~0ULL);
        vkResetFences(ctx.device, 1, &in_flight[current_frame]);

        uint32_t image_index;
        vkAcquireNextImageKHR(ctx.device, ctx.swapchain, ~0ULL, image_avail[current_frame], VK_NULL_HANDLE, &image_index);

        vkResetCommandBuffer(cmds[current_frame], 0);
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmds[current_frame], &begin);

        VkClearValue clear{ .color = { 0.15f, 0.05f, 0.0f, 1.0f } };
        VkRenderPassBeginInfo rp_begin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, render_pass, fbs[image_index], { {0,0}, ctx.swapchain_extent }, 1, &clear };
        vkCmdBeginRenderPass(cmds[current_frame], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmds[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkViewport vp{ 0, 0, float(ctx.swapchain_extent.width), float(ctx.swapchain_extent.height), 0.0f, 1.0f };
        VkRect2D scissor{ {0,0}, ctx.swapchain_extent };
        vkCmdSetViewport(cmds[current_frame], 0, 1, &vp);
        vkCmdSetScissor(cmds[current_frame], 0, 1, &scissor);
        vkCmdPushConstants(cmds[current_frame], pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &frame_counter);
        vkCmdDraw(cmds[current_frame], 3, 1, 0, 0);
        vkCmdEndRenderPass(cmds[current_frame]);
        vkEndCommandBuffer(cmds[current_frame]);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &image_avail[current_frame], &wait_stage, 1, &cmds[current_frame], 1, &render_done[current_frame] };
        vkQueueSubmit(ctx.graphics_queue, 1, &submit, in_flight[current_frame]);

        VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &render_done[current_frame], 1, &ctx.swapchain, &image_index };
        vkQueuePresentKHR(ctx.present_queue, &present);

        ++frame_counter;
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(ctx.device);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(ctx.device, image_avail[i], nullptr);
        vkDestroySemaphore(ctx.device, render_done[i], nullptr);
        vkDestroyFence(ctx.device, in_flight[i], nullptr);
    }
    for (auto fb : fbs) vkDestroyFramebuffer(ctx.device, fb, nullptr);
    vkDestroyCommandPool(ctx.device, pool, nullptr);
    vkDestroyPipeline(ctx.device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, nullptr);
    vkDestroyRenderPass(ctx.device, render_pass, nullptr);
    vkDestroyShaderModule(ctx.device, vert_mod, nullptr);
    vkDestroyShaderModule(ctx.device, frag_mod, nullptr);
    ctx.cleanup();
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    printf("Carrot Engine – Month 0 complete! Spinning orange triangle achieved!\n");
    return 0;
}
