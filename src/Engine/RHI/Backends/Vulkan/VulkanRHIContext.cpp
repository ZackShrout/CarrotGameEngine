//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanRHIContext.h"

#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "Window/Window.h"
#include "HotReload/ShaderWatcher.h"

namespace carrot::rhi::vulkan {
    // PUBLIC
    vulkan_rhi_context_t::vulkan_rhi_context_t(const rhi_desc_t& desc)
    {
        if (desc.api != graphics_api::vulkan)
            LOG_GRAPHICS_FATAL("Only Vulkan supported in this implementation");

        init(desc);
    }

    vulkan_rhi_context_t::~vulkan_rhi_context_t()
    {
        vkDeviceWaitIdle(_device ? _device->vk_device() : VK_NULL_HANDLE);

        _framebuffers = { };
        _graphics_pipeline.reset();
        _render_pass.reset();

        for (uint32_t i{ 0 }; i < _swapchain->get_image_count(); ++i)
        {
            // if (_image_available_semaphores[i]) vkDestroySemaphore(_device->vk_device(), _image_available_semaphores[i], nullptr);
            if (_render_finished_semaphores[i]) vkDestroySemaphore(_device->vk_device(), _render_finished_semaphores[i], nullptr);
        }

        _swapchain.reset();

        for (const auto& frame: _frames)
        {
            // if (frame.image_available) vkDestroySemaphore(_device->vk_device(), frame.image_available, nullptr);
            // if (frame.render_finished) vkDestroySemaphore(_device->vk_device(), frame.render_finished, nullptr);
            if (frame.image_acquire) vkDestroySemaphore(_device->vk_device(), frame.image_acquire, nullptr);
            if (frame.in_flight) vkDestroyFence(_device->vk_device(), frame.in_flight, nullptr);

            if (_command_pool != VK_NULL_HANDLE && frame.command_buffer)
                vkFreeCommandBuffers(_device->vk_device(), _command_pool, 1, &frame.command_buffer);
        }

        if (_command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(_device->vk_device(), _command_pool, nullptr);
            _command_pool = VK_NULL_HANDLE;
        }

        _graphics_queue.reset();
        _device.reset();

        if (_vk_surface) vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
        if (_vk_instance) vkDestroyInstance(_vk_instance, nullptr);
    }

    void vulkan_rhi_context_t::begin_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        // Wait for the previous use of this frame to finish
        VK_CHECK_FATAL(vkWaitForFences(
            _device->vk_device(),
            1,
            &frame.in_flight,
            VK_TRUE,
            UINT64_MAX
        ));

        // Reset the fence for this frame
        VK_CHECK_FATAL(vkResetFences(_device->vk_device(), 1, &frame.in_flight));

        // Hot-reload check: safe here because previous frames are done
        if (_pending_pipeline_reload)
        {
            LOG_GRAPHICS_INFO("Safe reload point reached — destroying old pipeline");
            _graphics_pipeline.reset();
            LOG_GRAPHICS_INFO("Old pipeline destroyed");
            _graphics_pipeline = std::make_unique<vulkan_pipeline_t>(_device.get(), _render_pass->vk_render_pass());
            _pending_pipeline_reload = false;
            LOG_GRAPHICS_INFO("Pipeline hot-reloaded (safe point)");
        }

        // Acquire next swapchain image
        const VkResult acquire_result{
            vkAcquireNextImageKHR(
                _device->vk_device(),
                _swapchain->vk_swapchain(),
                UINT64_MAX,
                frame.image_acquire,
                VK_NULL_HANDLE,
                &_current_image_index
            )
        };

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
        {
            recreate_swapchain_dependent_resources();
            LOG_GRAPHICS_WARN("Swapchain out of date/suboptimal - skipping frame");
            return;
        }

        if (acquire_result != VK_SUCCESS)
            VK_CHECK_FATAL(acquire_result);

        // Reset and begin command buffer
        VK_CHECK_FATAL(vkResetCommandBuffer(frame.command_buffer, 0));

        VkCommandBufferBeginInfo begin_info{ };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // or 0 if we reuse

        VK_CHECK_FATAL(vkBeginCommandBuffer(frame.command_buffer, &begin_info));

        // Begin render pass
        constexpr VkClearValue clear_color{ { { 0.02f, 0.02f, 0.04f, 1.0f } } };

        VkRenderPassBeginInfo rp_begin{ };
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass = _render_pass->vk_render_pass();
        rp_begin.framebuffer = _framebuffers[_current_image_index];
        rp_begin.renderArea.offset = { 0, 0 };
        rp_begin.renderArea.extent = _swapchain->extent();
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = &clear_color;

        vkCmdBeginRenderPass(frame.command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    void vulkan_rhi_context_t::record_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        // Set dynamic viewport and scissor
        VkViewport viewport{ };
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(_swapchain->get_width());
        viewport.height = static_cast<float>(_swapchain->get_height());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{ };
        scissor.offset = { 0, 0 };
        scissor.extent = { _swapchain->get_width(), _swapchain->get_height() };

        vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(frame.command_buffer, 0, 1, &scissor);

        // Bind the triangle pipeline
        vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline->vk_pipeline());

        _frame_counter++;

        // Push it!
        const uint32_t pc_data{ _frame_counter };
        vkCmdPushConstants(
            frame.command_buffer,
            _graphics_pipeline->vk_layout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0, // offset
            sizeof(uint32_t), // size must match shader
            &pc_data // pointer to the data
        );

        // Draw the hardcoded triangle (no vertex buffer needed)
        vkCmdDraw(frame.command_buffer, 3, 1, 0, 0);
    }

    void vulkan_rhi_context_t::end_frame()
    {
        const frame_resources_t& frame{ _frames[_current_frame] };

        // 1. End render pass (if not already ended in record_frame)
        vkCmdEndRenderPass(frame.command_buffer);

        // 2. End command buffer
        VK_CHECK_FATAL(vkEndCommandBuffer(frame.command_buffer));

        // 3. Submit to graphics queue
        VkPipelineStageFlags wait_stage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submit_info{ };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &frame.image_acquire;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &frame.command_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &_render_finished_semaphores[_current_image_index];

        VK_CHECK_FATAL(vkQueueSubmit(
            _device->graphics_queue(),
            1,
            &submit_info,
            frame.in_flight
        ));

        // 4. Present
        VkSwapchainKHR current_swapchain{ _swapchain->vk_swapchain() };

        VkPresentInfoKHR present_info{ };
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &_render_finished_semaphores[_current_image_index];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &current_swapchain;
        present_info.pImageIndices = &_current_image_index;

        const VkResult present_result{
            vkQueuePresentKHR(
                _device->graphics_queue(), // or separate present queue if different
                &present_info
            )
        };

        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
        {
            recreate_swapchain_dependent_resources();
            LOG_GRAPHICS_WARN("Present suboptimal/out of date");
        }
        else if (present_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(present_result);
        }

        // Advance frame index
        _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
    }

    void vulkan_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        vkDeviceWaitIdle(_device->vk_device());

        _swapchain->resize(width, height);

        // Re-create framebuffers with new images/views
        _framebuffers = _swapchain->create_framebuffers(_render_pass->vk_render_pass());

        LOG_GRAPHICS_INFO("Framebuffers recreated after resize");
    }

    void vulkan_rhi_context_t::wait_idle()
    {
        if (_graphics_queue) _graphics_queue->wait_idle();
    }

    // PRIVATE
    void vulkan_rhi_context_t::init(const rhi_desc_t& desc)
    {
        auto handle = window::get_native_handle();

        // ── 1. Create Vulkan Instance ─────────────────────────────────────────────

        std::vector<const char *> instance_extensions{
            VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(CARROT_PLATFORM_WAYLAND)
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif defined(CARROT_PLATFORM_WIN32)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

#ifdef _DEBUG
        if (desc.enable_debug_layers)
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        VkApplicationInfo app_info{ };
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Carrot Engine";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Carrot";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo inst_info{ };
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_info.pApplicationInfo = &app_info;
        inst_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
        inst_info.ppEnabledExtensionNames = instance_extensions.data();

#ifdef _DEBUG
        if (desc.enable_debug_layers)
        {
            const char* validation_layer{ "VK_LAYER_KHRONOS_validation" };
            inst_info.enabledLayerCount = 1;
            inst_info.ppEnabledLayerNames = &validation_layer;
        }
#endif

        VK_CHECK_FATAL(vkCreateInstance(&inst_info, nullptr, &_vk_instance));

        // ── 2. Create Wayland Surface ─────────────────────────────────────────────

#if defined(CARROT_PLATFORM_WAYLAND)
        VkWaylandSurfaceCreateInfoKHR surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surf_info.display = handle.wayland_t.display;
        surf_info.surface = handle.wayland_t.surface;
        VK_CHECK_FATAL(vkCreateWaylandSurfaceKHR(_vk_instance, &surf_info, nullptr, &_vk_surface));
#elif defined(CARROT_PLATFORM_WIN32)
        VkWin32SurfaceCreateInfoKHR surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surf_info.hwnd = static_cast<HWND>(handle.win32_t.hwnd);
        surf_info.hinstance = static_cast<HINSTANCE>(handle.win32_t.hinstance);
        vkCreateWin32SurfaceKHR(_vk_instance, &surf_info, nullptr, &_vk_surface);
#elif defined(CARROT_PLATFORM_COCOA)
#error Vulkan unsupported on MacOS currently
#endif

        // ── 3. Pick Physical Device & Queue Family ────────────────────────────────

        uint32_t device_count{ 0 };
        VK_CHECK_FATAL(vkEnumeratePhysicalDevices(_vk_instance, &device_count, nullptr));

        if (device_count == 0)
            LOG_GRAPHICS_FATAL("No Vulkan-capable physical devices found");

        std::vector<VkPhysicalDevice> physical_devices{ device_count };
        VK_CHECK_FATAL(vkEnumeratePhysicalDevices(_vk_instance, &device_count, physical_devices.data()));

        VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
        uint32_t graphics_family{ ~0u };

        for (auto pd: physical_devices)
        {
            uint32_t family{ ~0u };
            uint32_t qf_count{ 0 };
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, nullptr);

            std::vector<VkQueueFamilyProperties> families{ qf_count };
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, families.data());

            for (uint32_t i{ 0 }; i < qf_count; ++i)
            {
                VkBool32 present_support{ VK_FALSE };
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

        float priority{ 1.0f };
        VkDeviceQueueCreateInfo queue_info{ };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = graphics_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        const char* device_extensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo device_info{ };
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_extensions;

        VkDevice vk_device{ VK_NULL_HANDLE };
        VK_CHECK_FATAL(vkCreateDevice(physical_device, &device_info, nullptr, &vk_device));

        VkQueue graphics_queue{ VK_NULL_HANDLE };
        vkGetDeviceQueue(vk_device, graphics_family, 0, &graphics_queue);

        // ── 5. Wrap into VulkanDevice ─────────────────────────────────────────────

        _device = std::make_unique<vulkan_device_t>(vk_device, physical_device, graphics_family, graphics_queue,
                                                    _vk_surface);

        // ── Command Pool (shared, reset-able) ──────────────────────────────────────
        VkCommandPoolCreateInfo pool_info{ };
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = _device->graphics_family();

        VK_CHECK_FATAL(vkCreateCommandPool(
            _device->vk_device(),
            &pool_info,
            nullptr,
            &_command_pool
        ));

        // ── Allocate command buffers (one per frame in flight) ─────────────────────
        VkCommandBufferAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = _command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = k_max_frames_in_flight;

        std::array<VkCommandBuffer, k_max_frames_in_flight> cmd_buffers{ };
        VK_CHECK_FATAL(vkAllocateCommandBuffers(
            _device->vk_device(),
            &alloc_info,
            cmd_buffers.data()
        ));

        // ── Create per-frame synchronization primitives ────────────────────────────
        VkSemaphoreCreateInfo sem_info{ };
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{ };
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so first wait doesn't hang

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            _frames[i].command_buffer = cmd_buffers[i];

            // VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].image_available));
            // VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].render_finished));
            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].image_acquire));
            VK_CHECK_FATAL(vkCreateFence(_device->vk_device(), &fence_info, nullptr, &_frames[i].in_flight));
        }

        LOG_GRAPHICS_INFO("Created command pool, {} command buffers, and sync primitives", k_max_frames_in_flight);

        // ── 6. Create Swapchain ───────────────────────────────────────────────────
        _swapchain = std::make_unique<vulkan_swapchain_t>(
            _device.get(),
            _vk_surface,
            desc.width,
            desc.height
        );

        uint32_t image_count{ _swapchain->get_image_count() };
        // _image_available_semaphores.resize(image_count);
        _render_finished_semaphores.resize(image_count);

        for (uint32_t i{ 0 }; i < image_count; ++i)
        {
            // VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_image_available_semaphores[i]));
            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_render_finished_semaphores[i]));
        }

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

        hot_reload::shader_watcher_t::init([this](const std::string& changed_path) {
            LOG_GRAPHICS_INFO("Hot-reload QUEUED for {}", changed_path);
            if (_pending_pipeline_reload)
            {
                LOG_GRAPHICS_WARN("Hot-reload already pending — ignoring duplicate");
                return;
            }
            _pending_pipeline_reload = true;
        });

        LOG_CORE_INFO("VulkanRHIContext initialized successfully (fresh boot)");
    }

    void vulkan_rhi_context_t::recreate_swapchain_dependent_resources()
    {
        // Safety: wait for GPU to finish using old resources
        vkDeviceWaitIdle(_device->vk_device());

        // 1. Destroy old framebuffers (your RAII wrapper handles it)
        _framebuffers = framebuffer_array_t{ }; // or .clear() + reset device if needed

        // 2. Recreate swapchain (your existing resize logic)
        _swapchain->recreate();

        // 3. Recreate framebuffers attached to current render pass
        _framebuffers = _swapchain->create_framebuffers(_render_pass->vk_render_pass());

        LOG_GRAPHICS_INFO("Swapchain & framebuffers recreated after resize");
    }
} // namespace carrot::rhi::vulkan
