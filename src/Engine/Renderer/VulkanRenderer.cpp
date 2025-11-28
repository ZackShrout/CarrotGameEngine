// src/Engine/Renderer/VulkanRenderer.cpp
// Fully compliant with CARROT_MASTER_PLAN.txt and CODING_STANDARDS.md
// November 28, 2025 — Month 1 Day 1

#include "Engine/Renderer/VulkanRenderer.h"
#include "Engine/RHI/VulkanContext.h"
#include "Engine/Window/Window.h"

#include <cstdio>
#include <vector>
#include <array>

namespace carrot {

namespace {

constexpr uint32_t k_max_frames_in_flight{ 2 };

std::vector<uint32_t> load_spv(const char* filename) noexcept
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open shader %s\n", filename);
        return {};
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint32_t> buffer((size + 3) / 4);
    fread(buffer.data(), 1, size, f);
    fclose(f);
    return buffer;
}

} // anonymous namespace

// Static members
rhi::vulkan_context_t*          vulkan_renderer_t::_ctx{ nullptr };

VkPipeline                      vulkan_renderer_t::_graphics_pipeline{ VK_NULL_HANDLE };
VkPipelineLayout                vulkan_renderer_t::_pipeline_layout{ VK_NULL_HANDLE };
VkRenderPass                    vulkan_renderer_t::_render_pass{ VK_NULL_HANDLE };

VkCommandPool                   vulkan_renderer_t::_command_pool{ VK_NULL_HANDLE };
std::vector<VkCommandBuffer>    vulkan_renderer_t::_command_buffers;

std::vector<VkFramebuffer>      vulkan_renderer_t::_swapchain_framebuffers;

std::vector<VkSemaphore>        vulkan_renderer_t::_image_available_semaphores;
std::vector<VkSemaphore>        vulkan_renderer_t::_render_finished_semaphores;
std::vector<VkFence>            vulkan_renderer_t::_in_flight_fences;

uint32_t                        vulkan_renderer_t::_current_frame{ 0 };
uint32_t                        vulkan_renderer_t::_frame_counter{ 0 };
uint32_t                        vulkan_renderer_t::_current_image_index{ 0 };

void vulkan_renderer_t::init()
{
    _ctx = new rhi::vulkan_context_t;

    // ── Instance & Surface ─────────────────────────────────────
    VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = "Carrot Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Carrot";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    const char* instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo inst_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = 2;
    inst_info.ppEnabledExtensionNames = instance_extensions;

    VkInstance instance{ VK_NULL_HANDLE };
    vkCreateInstance(&inst_info, nullptr, &instance);

    // ── Surface from Wayland window ─────────────────────────────
    auto& win = window::get_primary_window();
    VkWaylandSurfaceCreateInfoKHR surf_info{ VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    surf_info.display = win.get_wl_display();
    surf_info.surface = win.get_wl_surface();

    VkSurfaceKHR surface{ VK_NULL_HANDLE };
    vkCreateWaylandSurfaceKHR(instance, &surf_info, nullptr, &surface);

    // ── Vulkan context (device, queues, swapchain) ───────────────
    _ctx->init(instance, surface);
    _ctx->create_swapchain(1280, 720);

    create_pipeline();
    recreate_swapchain_dependent_resources();
}

void vulkan_renderer_t::shutdown()
{
    vkDeviceWaitIdle(_ctx->device);

    destroy_pipeline();

    for (auto fb : _swapchain_framebuffers)
        vkDestroyFramebuffer(_ctx->device, fb, nullptr);
    _swapchain_framebuffers.clear();

    vkDestroyCommandPool(_ctx->device, _command_pool, nullptr);

    for (auto s : _image_available_semaphores) vkDestroySemaphore(_ctx->device, s, nullptr);
    for (auto s : _render_finished_semaphores) vkDestroySemaphore(_ctx->device, s, nullptr);
    for (auto f : _in_flight_fences) vkDestroyFence(_ctx->device, f, nullptr);

    _ctx->cleanup();

    vkDestroySurfaceKHR(_ctx->instance, _ctx->surface, nullptr);
    vkDestroyInstance(_ctx->instance, nullptr);

    delete _ctx;
    _ctx = nullptr;
}

void vulkan_renderer_t::create_pipeline()
{
    auto vert_spv = load_spv("bin/debug/shaders/triangle.spv");
    auto frag_spv = load_spv("bin/debug/shaders/triangle.frag.spv");

    VkShaderModule vert_module{ VK_NULL_HANDLE };
    VkShaderModule frag_module{ VK_NULL_HANDLE };

    VkShaderModuleCreateInfo mod_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    mod_info.codeSize = vert_spv.size() * sizeof(uint32_t);
    mod_info.pCode = vert_spv.data();
    vkCreateShaderModule(_ctx->device, &mod_info, nullptr, &vert_module);

    mod_info.codeSize = frag_spv.size() * sizeof(uint32_t);
    mod_info.pCode = frag_spv.data();
    vkCreateShaderModule(_ctx->device, &mod_info, nullptr, &frag_module);

    VkPushConstantRange push_range{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) };

    VkPipelineLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    vkCreatePipelineLayout(_ctx->device, &layout_info, nullptr, &_pipeline_layout);

    // ── Render Pass ─────────────────────────────────────────────
    VkAttachmentDescription color_att{};
    color_att.format = _ctx->swapchain_format;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkRenderPassCreateInfo rp_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &color_att;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    vkCreateRenderPass(_ctx->device, &rp_info, nullptr, &_render_pass);

    // ── Graphics Pipeline ───────────────────────────────────────
    VkPipelineShaderStageCreateInfo stages[2]{
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert_module, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, "main", nullptr }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.lineWidth = 1.f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;

    VkGraphicsPipelineCreateInfo pipe_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipe_info.stageCount = 2;
    pipe_info.pStages = stages;
    pipe_info.pVertexInputState = &vertex_input;
    pipe_info.pInputAssemblyState = &ia;
    pipe_info.pViewportState = &vp;
    pipe_info.pRasterizationState = &rs;
    pipe_info.pMultisampleState = &ms;
    pipe_info.pColorBlendState = &cb;
    pipe_info.pDynamicState = &dyn;
    pipe_info.layout = _pipeline_layout;
    pipe_info.renderPass = _render_pass;

    vkCreateGraphicsPipelines(_ctx->device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &_graphics_pipeline);

    vkDestroyShaderModule(_ctx->device, vert_module, nullptr);
    vkDestroyShaderModule(_ctx->device, frag_module, nullptr);
}

void vulkan_renderer_t::destroy_pipeline()
{
    if (_graphics_pipeline) vkDestroyPipeline(_ctx->device, _graphics_pipeline, nullptr);
    if (_pipeline_layout) vkDestroyPipelineLayout(_ctx->device, _pipeline_layout, nullptr);
    if (_render_pass) vkDestroyRenderPass(_ctx->device, _render_pass, nullptr);

    _graphics_pipeline = VK_NULL_HANDLE;
    _pipeline_layout = VK_NULL_HANDLE;
    _render_pass = VK_NULL_HANDLE;
}

void vulkan_renderer_t::recreate_swapchain_dependent_resources()
{
    vkDeviceWaitIdle(_ctx->device);

    // Destroy old framebuffers
    for (auto fb : _swapchain_framebuffers)
        vkDestroyFramebuffer(_ctx->device, fb, nullptr);
    _swapchain_framebuffers.clear();

    // Create framebuffers
    _swapchain_framebuffers.resize(_ctx->image_count);
    for (uint32_t i = 0; i < _ctx->image_count; ++i) {
        VkFramebufferCreateInfo fb_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fb_info.renderPass = _render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &_ctx->swapchain_views[i];
        fb_info.width = _ctx->swapchain_extent.width;
        fb_info.height = _ctx->swapchain_extent.height;
        fb_info.layers = 1;
        vkCreateFramebuffer(_ctx->device, &fb_info, nullptr, &_swapchain_framebuffers[i]);
    }

    // Command pool & buffers
    if (_command_pool) vkDestroyCommandPool(_ctx->device, _command_pool, nullptr);
    VkCommandPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = _ctx->graphics_family;
    vkCreateCommandPool(_ctx->device, &pool_info, nullptr, &_command_pool);

    _command_buffers.resize(k_max_frames_in_flight);
    VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.commandPool = _command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = k_max_frames_in_flight;
    vkAllocateCommandBuffers(_ctx->device, &alloc_info, _command_buffers.data());

    // Sync objects
    _image_available_semaphores.resize(k_max_frames_in_flight);
    _render_finished_semaphores.resize(k_max_frames_in_flight);
    _in_flight_fences.resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo sem_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
        vkCreateSemaphore(_ctx->device, &sem_info, nullptr, &_image_available_semaphores[i]);
        vkCreateSemaphore(_ctx->device, &sem_info, nullptr, &_render_finished_semaphores[i]);
        vkCreateFence(_ctx->device, &fence_info, nullptr, &_in_flight_fences[i]);
    }
}

void vulkan_renderer_t::begin_frame()
{
    vkWaitForFences(_ctx->device, 1, &_in_flight_fences[_current_frame], VK_TRUE, ~0ULL);
    vkResetFences(_ctx->device, 1, &_in_flight_fences[_current_frame]);

    uint32_t image_index{ 0 };
    VkResult result = vkAcquireNextImageKHR(_ctx->device, _ctx->swapchain, ~0ULL,
                                        _image_available_semaphores[_current_frame],
                                        VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: proper resize handling later
        recreate_swapchain_dependent_resources();
        return;
    }
    _current_image_index = image_index;

    vkResetCommandBuffer(_command_buffers[_current_frame], 0);

    VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(_command_buffers[_current_frame], &begin_info);

    VkClearValue clear_color{{{0.15f, 0.05f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo rp_begin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp_begin.renderPass = _render_pass;
    rp_begin.framebuffer = _swapchain_framebuffers[image_index];
    rp_begin.renderArea.extent = _ctx->swapchain_extent;
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = &clear_color;

    vkCmdBeginRenderPass(_command_buffers[_current_frame], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(_command_buffers[_current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    VkViewport viewport{ 0.f, 0.f,
        static_cast<float>(_ctx->swapchain_extent.width),
        static_cast<float>(_ctx->swapchain_extent.height), 0.f, 1.f };
    vkCmdSetViewport(_command_buffers[_current_frame], 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, _ctx->swapchain_extent};
    vkCmdSetScissor(_command_buffers[_current_frame], 0, 1, &scissor);

    vkCmdPushConstants(_command_buffers[_current_frame], _pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &_frame_counter);
}

void vulkan_renderer_t::render_frame()
{
    vkCmdDraw(_command_buffers[_current_frame], 3, 1, 0, 0);
    vkCmdEndRenderPass(_command_buffers[_current_frame]);
    vkEndCommandBuffer(_command_buffers[_current_frame]);
}

void vulkan_renderer_t::end_frame()
{
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &_image_available_semaphores[_current_frame];
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &_command_buffers[_current_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &_render_finished_semaphores[_current_frame];

    vkQueueSubmit(_ctx->graphics_queue, 1, &submit, _in_flight_fences[_current_frame]);

    VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &_render_finished_semaphores[_current_frame];
    present.swapchainCount = 1;
    present.pSwapchains = &_ctx->swapchain;
    present.pImageIndices = &_current_image_index; // set in begin_frame

    vkQueuePresentKHR(_ctx->present_queue, &present);

    ++_frame_counter;
    _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
}

void vulkan_renderer_t::reload_pipeline()
{
    vkDeviceWaitIdle(_ctx->device);
    destroy_pipeline();
    create_pipeline();
}

} // namespace carrot