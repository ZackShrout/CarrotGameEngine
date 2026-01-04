//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "VulkanRenderer.h"

#include "VulkanContext.h"
#include "Window/Window.h"
#include "Utils/ShaderUtils.h"
#include "Debug/DebugOverlay.h"
#include "Utils/Assert.h"

#include <vector>

namespace carrot::rhi::vulkan {
    namespace {

    } // anonymous namespace

    // PUBLIC
    void vulkan_renderer_t::init()
    {
        _ctx = new vulkan_context_t;

        // ── Instance & Surface ─────────────────────────────────────
        constexpr VkApplicationInfo app_info{
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            "Carrot Engine",
            VK_MAKE_VERSION(1, 0, 0),
            "Carrot",
            VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_3
        };

        const char* instance_extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
        };

        VkInstanceCreateInfo inst_info{ };
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_info.pApplicationInfo = &app_info;
        inst_info.enabledExtensionCount = 2;
        inst_info.ppEnabledExtensionNames = instance_extensions;

        VkInstance instance{ VK_NULL_HANDLE };
        vkCreateInstance(&inst_info, nullptr, &instance);

        // ── Surface from Wayland window ─────────────────────────────
        const auto& win = window::get_primary_window();
        VkWaylandSurfaceCreateInfoKHR surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surf_info.display = win.get_wl_display();
        surf_info.surface = win.get_wl_surface();

        VkSurfaceKHR surface{ VK_NULL_HANDLE };
        vkCreateWaylandSurfaceKHR(instance, &surf_info, nullptr, &surface);

        // ── Vulkan context (device, queues, swapchain) ───────────────
        _ctx->init(instance, surface);
        _ctx->create_swapchain(1280, 720);

        create_pipeline();

        // Create the shared command pool ONCE — used by renderer AND debug overlay
        VkCommandPoolCreateInfo pool_info{ };
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = _ctx->graphics_family();

        VkCommandPool raw_pool{ VK_NULL_HANDLE };
        vkCreateCommandPool(_ctx->device(), &pool_info, nullptr, &raw_pool);
        _command_pool = command_pool_t{ _ctx->device(), raw_pool };

        recreate_swapchain_dependent_resources();
    }

    void vulkan_renderer_t::shutdown()
    {
        vkDeviceWaitIdle(_ctx->device());

        destroy_pipeline();

        for (const auto fb: _swapchain_framebuffers)
            vkDestroyFramebuffer(_ctx->device(), fb, nullptr);
        _swapchain_framebuffers.clear();

        vkDestroyCommandPool(_ctx->device(), _command_pool.pool, nullptr);

        _command_pool = {};

        for (const auto& frame : _frames)
        {
            vkDestroySemaphore(_ctx->device(), frame.image_available, nullptr);
            vkDestroySemaphore(_ctx->device(), frame.render_finished, nullptr);
            vkDestroyFence(_ctx->device(), frame.in_flight, nullptr);
        }

        _ctx->cleanup();

        vkDestroySurfaceKHR(_ctx->instance(), _ctx->surface(), nullptr);
        vkDestroyInstance(_ctx->instance(), nullptr);

        delete _ctx;
        _ctx = nullptr;
    }
    void vulkan_renderer_t::begin_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        vkWaitForFences(_ctx->device(), 1, &frame.in_flight, VK_TRUE, ~0ULL);
        vkResetFences(_ctx->device(), 1, &frame.in_flight);

        uint32_t image_index{ 0 };

        const VkResult result{
            vkAcquireNextImageKHR(_ctx->device(), *_ctx->swapchain(), ~0ULL, frame.image_available, VK_NULL_HANDLE,
                                  &image_index)
        };

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // TODO: proper resize handling later
            recreate_swapchain_dependent_resources();
            return;
        }
        _current_image_index = image_index;

        vkResetCommandBuffer(frame.command_buffer, 0);

        VkCommandBufferBeginInfo begin_info{ };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(frame.command_buffer, &begin_info);

        constexpr VkClearValue clear_color{ { { 0.15f, 0.05f, 0.0f, 1.0f } } };

        VkRenderPassBeginInfo rp_begin{ };
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass = _render_pass.pass;
        rp_begin.framebuffer = _swapchain_framebuffers[image_index];
        rp_begin.renderArea.extent = _ctx->swapchain_extent();
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = &clear_color;

        vkCmdBeginRenderPass(frame.command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline.pipeline);

        const VkViewport viewport{
            0.f, 0.f,
            static_cast<float>(_ctx->swapchain_extent().width),
            static_cast<float>(_ctx->swapchain_extent().height), 0.f, 1.f
        };
        vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);

        const VkRect2D scissor{ { 0, 0 }, _ctx->swapchain_extent() };
        vkCmdSetScissor(frame.command_buffer, 0, 1, &scissor);

        vkCmdPushConstants(frame.command_buffer, _pipeline_layout.layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &_frame_counter);
    }
    void vulkan_renderer_t::render_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        vkCmdDraw(frame.command_buffer, 3, 1, 0, 0);

        // Debug overlay — always last in the render pass
        // ← ONLY RENDER DEBUG OVERLAY AFTER IT'S INITIALIZED
        if (debug::is_initialized()) debug::render(get_current_command_buffer());

        vkCmdEndRenderPass(frame.command_buffer);
    }

    void vulkan_renderer_t::end_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        vkEndCommandBuffer(frame.command_buffer);

        constexpr VkPipelineStageFlags wait_stage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submit{ };
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.image_available;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &frame.command_buffer;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &frame.render_finished;

        vkQueueSubmit(_ctx->graphics_queue(), 1, &submit, frame.in_flight);

        VkPresentInfoKHR present{ };
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &frame.render_finished;
        present.swapchainCount = 1;
        present.pSwapchains = _ctx->swapchain();
        present.pImageIndices = &_current_image_index; // set in begin_frame

        vkQueuePresentKHR(_ctx->present_queue(), &present);

        ++_frame_counter;
        _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
    }
    void vulkan_renderer_t::reload_pipeline()
    {
        vkDeviceWaitIdle(_ctx->device());
        destroy_pipeline();
        create_pipeline();
    }

    // PRIVATE
    void vulkan_renderer_t::create_pipeline()
    {
        auto vert_spv = load_spv("shaders/triangle.vert.spv");
        auto frag_spv = load_spv("shaders/triangle.frag.spv");

        VkShaderModule vert_module{ VK_NULL_HANDLE };
        VkShaderModule frag_module{ VK_NULL_HANDLE };

        VkShaderModuleCreateInfo mod_info{ };
        mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        mod_info.codeSize = vert_spv.size() * sizeof(uint32_t);
        mod_info.pCode = vert_spv.data();
        vkCreateShaderModule(_ctx->device(), &mod_info, nullptr, &vert_module);

        mod_info.codeSize = frag_spv.size() * sizeof(uint32_t);
        mod_info.pCode = frag_spv.data();
        vkCreateShaderModule(_ctx->device(), &mod_info, nullptr, &frag_module);

        VkPushConstantRange push_range{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) };

        VkPipelineLayoutCreateInfo layout_info{ };
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_range;

        VkPipelineLayout raw_layout{ VK_NULL_HANDLE };
        vkCreatePipelineLayout(_ctx->device(), &layout_info, nullptr, &raw_layout);
        _pipeline_layout = pipeline_layout_t{ _ctx->device(), raw_layout };

        // ── Render Pass ─────────────────────────────────────────────
        VkAttachmentDescription color_att{ };
        color_att.format = _ctx->swapchain_format();
        color_att.samples = VK_SAMPLE_COUNT_1_BIT;
        color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{ };
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        VkRenderPassCreateInfo rp_info{ };
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = &color_att;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;

        VkRenderPass raw_rp{ VK_NULL_HANDLE };
        vkCreateRenderPass(_ctx->device(), &rp_info, nullptr, &raw_rp);
        _render_pass = render_pass_t{ _ctx->device(), raw_rp };

        // Share the render pass with the debug overlay
        vulkan_context_t::get()->set_render_pass(_render_pass.pass);

        // ── Graphics Pipeline ───────────────────────────────────────
        VkPipelineShaderStageCreateInfo stages[2]{
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
                vert_module, "main", nullptr
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
                frag_module, "main", nullptr
            }
        };

        VkPipelineVertexInputStateCreateInfo vertex_input{ };
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo ia{ };
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp{ };
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{ };
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.lineWidth = 1.f;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo ms{ };
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blend{ };
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb{ };
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &blend;

        VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{ };
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates = dyn_states;

        VkGraphicsPipelineCreateInfo pipe_info{ };
        pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipe_info.stageCount = 2;
        pipe_info.pStages = stages;
        pipe_info.pVertexInputState = &vertex_input;
        pipe_info.pInputAssemblyState = &ia;
        pipe_info.pViewportState = &vp;
        pipe_info.pRasterizationState = &rs;
        pipe_info.pMultisampleState = &ms;
        pipe_info.pColorBlendState = &cb;
        pipe_info.pDynamicState = &dyn;
        pipe_info.layout = _pipeline_layout.layout;
        pipe_info.renderPass = _render_pass.pass;

        VkPipeline raw_pipe{ VK_NULL_HANDLE };
        vkCreateGraphicsPipelines(_ctx->device(), VK_NULL_HANDLE, 1, &pipe_info, nullptr, &raw_pipe);
        _graphics_pipeline = pipeline_t{ _ctx->device(), raw_pipe };

        vkDestroyShaderModule(_ctx->device(), vert_module, nullptr);
        vkDestroyShaderModule(_ctx->device(), frag_module, nullptr);
    }
    void vulkan_renderer_t::destroy_pipeline()
    {
        _graphics_pipeline = {};
        _pipeline_layout = {};
        _render_pass = {};
    }
    void vulkan_renderer_t::recreate_swapchain_dependent_resources()
    {
        vkDeviceWaitIdle(_ctx->device());

        // Framebuffers: auto-cleaned by RAII destructor
        _swapchain_framebuffers = framebuffer_array_t{ _ctx->device() };
        _swapchain_framebuffers.resize(_ctx->image_count());

        for (uint32_t i = 0; i < _ctx->image_count(); ++i)
        {
            VkFramebufferCreateInfo fb_info{ };
            fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_info.renderPass = _render_pass.pass;
            fb_info.attachmentCount = 1;
            fb_info.pAttachments = &_ctx->swapchain_views()[i];
            fb_info.width = _ctx->swapchain_extent().width;
            fb_info.height = _ctx->swapchain_extent().height;
            fb_info.layers = 1;

            const VkResult result{ vkCreateFramebuffer(_ctx->device(), &fb_info, nullptr, &_swapchain_framebuffers[i]) };
            // TODO: proper error checking
            CE_ASSERT(result == VK_SUCCESS, "Failed to create framebuffer");
        }

        // Destroy old per-frame sync objects
        for (auto& frame : _frames)
        {
            if (frame.image_available != VK_NULL_HANDLE) vkDestroySemaphore(_ctx->device(), frame.image_available, nullptr);
            if (frame.render_finished != VK_NULL_HANDLE) vkDestroySemaphore(_ctx->device(), frame.render_finished, nullptr);
            if (frame.in_flight != VK_NULL_HANDLE) vkDestroyFence(_ctx->device(), frame.in_flight, nullptr);

            frame.image_available = VK_NULL_HANDLE;
            frame.render_finished = VK_NULL_HANDLE;
            frame.in_flight = VK_NULL_HANDLE;
        }

        // Only recreate command buffers — pool stays alive
        if (!_frames.empty())
        {
            vkFreeCommandBuffers(_ctx->device(), _command_pool.pool, static_cast<uint32_t>(_frames.size()),
                                 &_frames[0].command_buffer);
        }

        VkCommandBufferAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = _command_pool.pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = k_max_frames_in_flight;

        std::array<VkCommandBuffer, k_max_frames_in_flight> cmd_buffers{};
        vkAllocateCommandBuffers(_ctx->device(), &alloc_info, cmd_buffers.data());

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            _frames[i].command_buffer = cmd_buffers[i];
        }

        // Recreate sync objects
        VkSemaphoreCreateInfo sem_info{ };
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{ };
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto& frame : _frames)
        {
            vkCreateSemaphore(_ctx->device(), &sem_info, nullptr, &frame.image_available);
            vkCreateSemaphore(_ctx->device(), &sem_info, nullptr, &frame.render_finished);
            vkCreateFence(_ctx->device(), &fence_info, nullptr, &frame.in_flight);
        }
    }
} // namespace carrot::rhi::vulkan

namespace carrot::renderer {
    renderer_t* create_backend()
    {
        static rhi::vulkan::vulkan_renderer_t instance;
        return &instance;
    }
}