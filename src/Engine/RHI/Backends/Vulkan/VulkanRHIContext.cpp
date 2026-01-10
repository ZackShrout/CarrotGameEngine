//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "VulkanRHIContext.h"

#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "Window/Window.h"
#include "Core/Platform/Wayland/WaylandWindow.h"

namespace carrot::rhi::vulkan {
    // vulkan_rhi_context_t::vulkan_rhi_context_t(vulkan::vulkan_renderer_t* existing_renderer)
    //     : _legacy_renderer(existing_renderer)
    // {
    //     // Extract from legacy
    //     vulkan_context_t* ctx{ existing_renderer->get_context() };
    //
    //     _device = std::make_unique<vulkan_device_t>(ctx);
    //
    //     _swapchain = std::unique_ptr<vulkan_swapchain_t>(
    //         dynamic_cast<vulkan_swapchain_t *>(
    //             _device->create_swapchain(nullptr, ctx->swapchain_extent().width, ctx->swapchain_extent().height)
    //         )
    //     );
    //
    //     _graphics_queue = std::make_unique<vulkan_command_queue_t>(ctx->graphics_queue(), ctx->graphics_family());
    // }

    // PUBLIC
    vulkan_rhi_context_t::vulkan_rhi_context_t(const rhi_desc_t& desc)
    {
        if (desc.api != graphics_api::vulkan)
            LOG_GRAPHICS_FATAL("Only Vulkan supported in this implementation");

        if (!desc.window_handle)
            LOG_GRAPHICS_FATAL("No native window handle provided for Vulkan surface");

        init(desc);
    }

    vulkan_rhi_context_t::~vulkan_rhi_context_t()
    {
        vkDeviceWaitIdle(_device ? _device->vk_device() : VK_NULL_HANDLE);

        // Order matters - destroy in reverse creation order
        _swapchain.reset();

        // Destroy sync objects first
        for (const auto& frame : _frames)
        {
            if (frame.image_available) vkDestroySemaphore(_device->vk_device(), frame.image_available, nullptr);
            if (frame.render_finished) vkDestroySemaphore(_device->vk_device(), frame.render_finished, nullptr);
            if (frame.in_flight)       vkDestroyFence(_device->vk_device(), frame.in_flight, nullptr);
        }

        // Free command buffers + destroy pool
        if (_command_pool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(_device->vk_device(), _command_pool, k_max_frames_in_flight, &_frames[0].command_buffer);
            vkDestroyCommandPool(_device->vk_device(), _command_pool, nullptr);
            _command_pool = VK_NULL_HANDLE;
        }

        _graphics_queue.reset();
        _device.reset();

        if (_vk_surface)   vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
        if (_vk_instance)  vkDestroyInstance(_vk_instance, nullptr);
    }

    void vulkan_rhi_context_t::begin_frame()
    {
        auto& frame = _frames[_current_frame];

        // 1. Wait for previous use of this frame to finish
        VK_CHECK_FATAL(vkWaitForFences(
            _device->vk_device(),
            1,
            &frame.in_flight,
            VK_TRUE,
            UINT64_MAX
        ));

        // 2. Reset the fence for this frame
        VK_CHECK_FATAL(vkResetFences(_device->vk_device(), 1, &frame.in_flight));

        // 3. Acquire next swapchain image
        VkResult acquire_result = vkAcquireNextImageKHR(
            _device->vk_device(),
            _swapchain->vk_swapchain(),
            UINT64_MAX,
            frame.image_available,
            VK_NULL_HANDLE,
            &_current_image_index
        );

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
        {
            // TODO: Handle resize/recreate swapchain (later step)
            LOG_GRAPHICS_WARN("Swapchain out of date/suboptimal - skipping frame");
            return;
        }
        else if (acquire_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(acquire_result);
        }

        // 4. Reset and begin command buffer
        VK_CHECK_FATAL(vkResetCommandBuffer(frame.command_buffer, 0));

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // or 0 if you reuse

        VK_CHECK_FATAL(vkBeginCommandBuffer(frame.command_buffer, &begin_info));

        // 5. Begin render pass
        VkClearValue clear_color = {{{0.02f, 0.02f, 0.04f, 1.0f}}};  // nice dark background

        VkRenderPassBeginInfo rp_begin{};
        rp_begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass        = _render_pass->vk_render_pass();
        rp_begin.framebuffer       = _framebuffers[_current_image_index];
        rp_begin.renderArea.offset = {0, 0};
        rp_begin.renderArea.extent = _swapchain->extent();
        rp_begin.clearValueCount   = 1;
        rp_begin.pClearValues      = &clear_color;

        vkCmdBeginRenderPass(frame.command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        // LOG_GRAPHICS_TRACE("Frame {} acquired image {}", _current_frame, _current_image_index);
    }

    void vulkan_rhi_context_t::record_frame()
    {
        auto& frame = _frames[_current_frame];

        // Set dynamic viewport and scissor (since we marked them dynamic)
        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(_swapchain->get_width());
        viewport.height   = static_cast<float>(_swapchain->get_height());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = { _swapchain->get_width(), _swapchain->get_height() };

        vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(frame.command_buffer, 0, 1, &scissor);

        // Bind the triangle pipeline
        vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline->vk_pipeline());

        // Use a member or static counter — here using a simple static for demo
        static uint32_t frame_counter = 0;
        frame_counter++;

        // Push it!
        uint32_t pc_data = frame_counter;
        vkCmdPushConstants(
            frame.command_buffer,
            _graphics_pipeline->vk_layout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,                       // offset
            sizeof(uint32_t),        // size must match shader
            &pc_data                 // pointer to the data
        );

        // Draw the hardcoded triangle (no vertex buffer needed)
        vkCmdDraw(frame.command_buffer, 3, 1, 0, 0);
    }

    void vulkan_rhi_context_t::end_frame()
    {
        auto& frame = _frames[_current_frame];

        // 1. End render pass (if not already ended in record_frame)
        vkCmdEndRenderPass(frame.command_buffer);

        // 2. End command buffer
        VK_CHECK_FATAL(vkEndCommandBuffer(frame.command_buffer));

        // 3. Submit to graphics queue
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info{};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &frame.image_available;
        submit_info.pWaitDstStageMask    = &wait_stage;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &frame.command_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &frame.render_finished;

        VK_CHECK_FATAL(vkQueueSubmit(
            _device->graphics_queue(),
            1,
            &submit_info,
            frame.in_flight
        ));

        // 4. Present
        VkSwapchainKHR current_swapchain{ _swapchain->vk_swapchain() };

        VkPresentInfoKHR present_info{};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &frame.render_finished;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &current_swapchain;
        present_info.pImageIndices      = &_current_image_index;

        VkResult present_result = vkQueuePresentKHR(
            _device->graphics_queue(),  // or separate present queue if different
            &present_info
        );

        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
        {
            // TODO: recreate swapchain on resize
            LOG_GRAPHICS_WARN("Present suboptimal/out of date");
        }
        else if (present_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(present_result);
        }

        // 5. Advance frame index
        _current_frame = (_current_frame + 1) % k_max_frames_in_flight;

        // LOG_GRAPHICS_TRACE("Frame {} submitted & presented", (_current_frame + k_max_frames_in_flight - 1) % k_max_frames_in_flight);
    }

    void vulkan_rhi_context_t::wait_idle()
    {
        if (_graphics_queue) _graphics_queue->wait_idle();
    }

    // PRIVATE
    void vulkan_rhi_context_t::init(const rhi_desc_t& desc)
    {
        auto* wl_display_ = window::get_primary_window().get_wl_display();
        auto* wl_surface_ = static_cast<wl_surface*>(desc.window_handle);
        
        // ── 1. Create Vulkan Instance ─────────────────────────────────────────────

        std::vector<const char*> instance_extensions{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
        };

#ifdef _DEBUG
        if (desc.enable_debug_layers)
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Carrot Engine";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Carrot";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo inst_info{};
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_info.pApplicationInfo = &app_info;
        inst_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
        inst_info.ppEnabledExtensionNames = instance_extensions.data();

#ifdef _DEBUG
        if (desc.enable_debug_layers)
        {
            const char* validation_layer = "VK_LAYER_KHRONOS_validation";
            inst_info.enabledLayerCount = 1;
            inst_info.ppEnabledLayerNames = &validation_layer;
        }
#endif

        VK_CHECK_FATAL(vkCreateInstance(&inst_info, nullptr, &_vk_instance));

        // ── 2. Create Wayland Surface ─────────────────────────────────────────────

        VkWaylandSurfaceCreateInfoKHR surf_info{};
        surf_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surf_info.display = wl_display_;
        surf_info.surface = wl_surface_;

        VK_CHECK_FATAL(vkCreateWaylandSurfaceKHR(_vk_instance, &surf_info, nullptr, &_vk_surface));

        // ── 3. Pick Physical Device & Queue Family ────────────────────────────────

        uint32_t device_count = 0;
        VK_CHECK_FATAL(vkEnumeratePhysicalDevices(_vk_instance, &device_count, nullptr));

        if (device_count == 0)
            LOG_GRAPHICS_FATAL("No Vulkan-capable physical devices found");

        std::vector<VkPhysicalDevice> physical_devices(device_count);
        VK_CHECK_FATAL(vkEnumeratePhysicalDevices(_vk_instance, &device_count, physical_devices.data()));

        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t graphics_family = ~0u;

        for (auto pd : physical_devices)
        {
            uint32_t family = ~0u;
            uint32_t qf_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, nullptr);

            std::vector<VkQueueFamilyProperties> families(qf_count);
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, families.data());

            for (uint32_t i = 0; i < qf_count; ++i)
            {
                VkBool32 present_support = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, _vk_surface, &present_support);

                if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support)
                {
                    family = i;
                    break;
                }
            }

            if (family != ~0u)
            {
                physical_device = pd;
                graphics_family = family;
                break;
            }
        }

        if (!physical_device)
            LOG_GRAPHICS_FATAL("No physical device supports both graphics and presentation");

        // ── 4. Create Logical Device + Queues ─────────────────────────────────────

        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = graphics_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_extensions;

        VkDevice vk_device = VK_NULL_HANDLE;
        VK_CHECK_FATAL(vkCreateDevice(physical_device, &device_info, nullptr, &vk_device));

        VkQueue graphics_queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(vk_device, graphics_family, 0, &graphics_queue);

        // ── 5. Wrap into VulkanDevice ─────────────────────────────────────────────

        _device = std::make_unique<vulkan_device_t>(vk_device, physical_device, graphics_family, graphics_queue, _vk_surface);

        // ── Command Pool (shared, reset-able) ──────────────────────────────────────
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = _device->graphics_family();

        VK_CHECK_FATAL(vkCreateCommandPool(
            _device->vk_device(),
            &pool_info,
            nullptr,
            &_command_pool
        ));

        // ── Allocate command buffers (one per frame in flight) ─────────────────────
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = _command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = k_max_frames_in_flight;

        std::array<VkCommandBuffer, k_max_frames_in_flight> cmd_buffers{};
        VK_CHECK_FATAL(vkAllocateCommandBuffers(
            _device->vk_device(),
            &alloc_info,
            cmd_buffers.data()
        ));

        // ── Create per-frame synchronization primitives ────────────────────────────
        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // so first wait doesn't hang

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            _frames[i].command_buffer = cmd_buffers[i];

            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].image_available));
            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].render_finished));
            VK_CHECK_FATAL(vkCreateFence(_device->vk_device(), &fence_info, nullptr, &_frames[i].in_flight));
        }

        LOG_GRAPHICS_INFO("Created command pool, {} command buffers, and sync primitives", k_max_frames_in_flight);

        // ── 6. Create Swapchain ───────────────────────────────────────────────────
        _swapchain = std::make_unique<vulkan_swapchain_t>(
            _device.get(),
            _vk_surface,
            desc.window_handle,
            desc.width,
            desc.height
        );

        // ── 7. Create Render Pass ─────────────────────────────────────────────────
        _render_pass = std::make_unique<vulkan_render_pass_t>(
            _device.get(),
            _swapchain->format()
        );

        // ── 8. Create Graphics Pipeline ───────────────────────────────────────────
        _graphics_pipeline = std::make_unique<vulkan_pipeline_t>(
            _device.get(),
            _render_pass->vk_render_pass()
        );

        // ── 9. Create Framebuffers ────────────────────────────────────────────────
        _framebuffers = _swapchain->create_framebuffers(_render_pass->vk_render_pass());

        // ── 10. Create Graphics Queue Wrapper ─────────────────────────────────────
        _graphics_queue = std::make_unique<vulkan_command_queue_t>(graphics_queue, graphics_family);

        LOG_CORE_INFO("VulkanRHIContext initialized successfully (fresh boot)");
    }
} // namespace carrot::rhi::vulkan
