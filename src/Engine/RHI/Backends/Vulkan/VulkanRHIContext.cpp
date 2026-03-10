//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanRHIContext.h"

#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanTexture.h"
#include "Window/Window.h"
#include "HotReload/ShaderWatcher.h"

namespace carrot::rhi::vulkan {
    // PUBLIC
    vulkan_rhi_context_t::vulkan_rhi_context_t(const rhi_desc_t& desc)
    {
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
            if (_render_finished_semaphores[i])
                vkDestroySemaphore(_device->vk_device(), _render_finished_semaphores[i],
                                   nullptr);
        }

        _swapchain.reset();

        for (const auto& frame: _frames)
        {
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
        _skip_frame = false;
        _frame_active = false;
        _render_pass_active = false;

        if (_swapchain_dirty)
        {
            recreate_swapchain_dependent_resources();
            _swapchain_dirty = false;
            _skip_frame = true;
            return;
        }

        const frame_resources_t& frame{ _frames[_current_frame] };

        // Wait for the previous use of this frame to finish
        VK_CHECK_FATAL(vkWaitForFences(_device->vk_device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));

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

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            LOG_GRAPHICS_WARN("Swapchain out of date/suboptimal - skipping frame");
            _swapchain_dirty = true;
            _skip_frame = true;
            return;
        }

        if (acquire_result == VK_SUBOPTIMAL_KHR)
        {
            _swapchain_dirty = true;
            // but continue this frame
        }
        else if (acquire_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(acquire_result);
        }

        // Reset the fence for this frame
        VK_CHECK_FATAL(vkResetFences(_device->vk_device(), 1, &frame.in_flight));

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
        _render_pass_active = true;
        _frame_active = true;
    }

    void vulkan_rhi_context_t::record_frame()
    {
        if (_skip_frame || !_frame_active)
            return;

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

        vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline->vk_pipeline());

        _frame_counter++;

        const uint32_t pc_data{ _frame_counter };
        vkCmdPushConstants(
            frame.command_buffer,
            _graphics_pipeline->vk_layout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0, // offset
            sizeof(uint32_t), // size must match shader
            &pc_data // pointer to the data
        );

        vkCmdDraw(frame.command_buffer, 3, 1, 0, 0);
    }

    void vulkan_rhi_context_t::end_frame()
    {
        if (_skip_frame || !_frame_active)
            return;

        const frame_resources_t& frame{ _frames[_current_frame] };

        // 1. End render pass (if not already ended in record_frame)
        if (_render_pass_active)
        {
            vkCmdEndRenderPass(frame.command_buffer);
            _render_pass_active = false;
        }

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

        VK_CHECK_FATAL(vkQueueSubmit(_device->graphics_queue(), 1, &submit_info, frame.in_flight));

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
            _swapchain_dirty = true;
            LOG_GRAPHICS_WARN("Present suboptimal/out of date");
        }
        else if (present_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(present_result);
        }

        // Advance frame index
        _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
        _frame_active = false;
    }

    void vulkan_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            _pending_resize_width = 0;
            _pending_resize_height = 0;
            _swapchain_dirty = true;
            return;
        }

        _pending_resize_width = width;
        _pending_resize_height = height;
        _swapchain_dirty = true;
    }

    std::unique_ptr<rhi_texture_t> vulkan_rhi_context_t::create_texture_2d(const texture_create_info_t& info)
    {
        if (info.width == 0 || info.height == 0)
        {
            LOG_GRAPHICS_ERROR("create_texture_2d failed: invalid dimensions {}x{}", info.width, info.height);
            return nullptr;
        }

        if (info.initial_data == nullptr || info.initial_data_size == 0)
        {
            LOG_GRAPHICS_ERROR("create_texture_2d failed: initial_data was null or empty");
            return nullptr;
        }

        VkDevice device{ _device->vk_device() };

        const VkFormat vk_format{
            info.format == texture_format_t::rgba8_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM
        };

        const VkDeviceSize upload_size{ static_cast<VkDeviceSize>(info.initial_data_size) };

        // -------------------------------------------------------------------------
        // 1. Create staging buffer
        // -------------------------------------------------------------------------
        VkBuffer staging_buffer{ VK_NULL_HANDLE };
        VkDeviceMemory staging_memory{ VK_NULL_HANDLE };

        {
            VkBufferCreateInfo buffer_info{ };
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = upload_size;
            buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not create staging buffer");
                return nullptr;
            }

            VkMemoryRequirements mem_requirements{ };
            vkGetBufferMemoryRequirements(device, staging_buffer, &mem_requirements);

            VkMemoryAllocateInfo alloc_info{ };
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = mem_requirements.size;
            alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(device, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not allocate staging buffer memory");
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }

            vkBindBufferMemory(device, staging_buffer, staging_memory, 0);

            void* mapped{ nullptr };
            if (vkMapMemory(device, staging_memory, 0, upload_size, 0, &mapped) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not map staging buffer memory");
                vkFreeMemory(device, staging_memory, nullptr);
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }

            std::memcpy(mapped, info.initial_data, static_cast<size_t>(upload_size));
            vkUnmapMemory(device, staging_memory);
        }

        // -------------------------------------------------------------------------
        // 2. Create image
        // -------------------------------------------------------------------------
        VkImage image{ VK_NULL_HANDLE };
        VkDeviceMemory image_memory{ VK_NULL_HANDLE };

        {
            VkImageCreateInfo image_info{ };
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.extent.width = info.width;
            image_info.extent.height = info.height;
            image_info.extent.depth = 1;
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            image_info.format = vk_format;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_info.samples = VK_SAMPLE_COUNT_1_BIT;

            if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not create VkImage");
                vkFreeMemory(device, staging_memory, nullptr);
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }

            VkMemoryRequirements mem_requirements{ };
            vkGetImageMemoryRequirements(device, image, &mem_requirements);

            VkMemoryAllocateInfo alloc_info{ };
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = mem_requirements.size;
            alloc_info.memoryTypeIndex = find_memory_type(
                mem_requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not allocate image memory");
                vkDestroyImage(device, image, nullptr);
                vkFreeMemory(device, staging_memory, nullptr);
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }

            vkBindImageMemory(device, image, image_memory, 0);
        }

        // -------------------------------------------------------------------------
        // 3. Upload staging buffer -> image with layout transitions
        // -------------------------------------------------------------------------
        {
            VkCommandBuffer cmd{ begin_single_time_commands() };

            VkImageMemoryBarrier to_transfer_dst{ };
            to_transfer_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_transfer_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            to_transfer_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_transfer_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_transfer_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_transfer_dst.image = image;
            to_transfer_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_transfer_dst.subresourceRange.baseMipLevel = 0;
            to_transfer_dst.subresourceRange.levelCount = 1;
            to_transfer_dst.subresourceRange.baseArrayLayer = 0;
            to_transfer_dst.subresourceRange.layerCount = 1;
            to_transfer_dst.srcAccessMask = 0;
            to_transfer_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &to_transfer_dst);

            VkBufferImageCopy region{ };
            region.bufferOffset = 0;
            region.bufferRowLength = 0; // tightly packed
            region.bufferImageHeight = 0; // tightly packed
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { info.width, info.height, 1 };

            vkCmdCopyBufferToImage(cmd, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier to_shader_read{ };
            to_shader_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.image = image;
            to_shader_read.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader_read.subresourceRange.baseMipLevel = 0;
            to_shader_read.subresourceRange.levelCount = 1;
            to_shader_read.subresourceRange.baseArrayLayer = 0;
            to_shader_read.subresourceRange.layerCount = 1;
            to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_shader_read);

            end_single_time_commands(cmd);
        }

        // -------------------------------------------------------------------------
        // 4. Create image view
        // -------------------------------------------------------------------------
        VkImageView image_view{ VK_NULL_HANDLE };
        {
            VkImageViewCreateInfo view_info{ };
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = vk_format;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &view_info, nullptr, &image_view) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not create image view");

                vkDestroyImage(device, image, nullptr);
                vkFreeMemory(device, image_memory, nullptr);
                vkFreeMemory(device, staging_memory, nullptr);
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }
        }

        // -------------------------------------------------------------------------
        // 5. Create sampler
        // -------------------------------------------------------------------------
        VkSampler sampler{ VK_NULL_HANDLE };
        {
            VkSamplerCreateInfo sampler_info{ };
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_NEAREST;
            sampler_info.minFilter = VK_FILTER_NEAREST;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.maxAnisotropy = 1.f;
            sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            sampler_info.unnormalizedCoordinates = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.mipLodBias = 0.f;
            sampler_info.minLod = 0.f;
            sampler_info.maxLod = 0.f;

            if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) != VK_SUCCESS)
            {
                LOG_GRAPHICS_ERROR("create_texture_2d failed: could not create sampler");

                vkDestroyImageView(device, image_view, nullptr);
                vkDestroyImage(device, image, nullptr);
                vkFreeMemory(device, image_memory, nullptr);
                vkFreeMemory(device, staging_memory, nullptr);
                vkDestroyBuffer(device, staging_buffer, nullptr);
                return nullptr;
            }
        }

        // staging resources are no longer needed after upload
        vkFreeMemory(device, staging_memory, nullptr);
        vkDestroyBuffer(device, staging_buffer, nullptr);

        auto texture = std::make_unique<vulkan_texture_t>(_device.get());
        texture->set_width(info.width);
        texture->set_height(info.height);
        texture->set_format(info.format);
        texture->set_image(image);
        texture->set_memory(image_memory);
        texture->set_view(image_view);
        texture->set_sampler(sampler);

        LOG_GRAPHICS_INFO("Created Vulkan texture: {}x{}", info.width, info.height);

        return texture;
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

        VK_CHECK_FATAL(vkCreateCommandPool(_device->vk_device(), &pool_info, nullptr, &_command_pool));

        // ── Allocate command buffers (one per frame in flight) ─────────────────────
        VkCommandBufferAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = _command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = k_max_frames_in_flight;

        std::array<VkCommandBuffer, k_max_frames_in_flight> cmd_buffers{ };
        VK_CHECK_FATAL(vkAllocateCommandBuffers(_device->vk_device(), &alloc_info, cmd_buffers.data()));

        // ── Create per-frame synchronization primitives ────────────────────────────
        VkSemaphoreCreateInfo sem_info{ };
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{ };
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so first wait doesn't hang

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            _frames[i].command_buffer = cmd_buffers[i];

            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_frames[i].image_acquire));
            VK_CHECK_FATAL(vkCreateFence(_device->vk_device(), &fence_info, nullptr, &_frames[i].in_flight));
        }

        LOG_GRAPHICS_INFO("Created command pool, {} command buffers, and sync primitives", k_max_frames_in_flight);

        // ── 6. Create Swapchain ───────────────────────────────────────────────────
        _swapchain = std::make_unique<vulkan_swapchain_t>(_device.get(), _vk_surface, desc.width, desc.height);

        uint32_t image_count{ _swapchain->get_image_count() };
        _render_finished_semaphores.resize(image_count);

        for (uint32_t i{ 0 }; i < image_count; ++i)
        {
            VK_CHECK_FATAL(
                vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_render_finished_semaphores[i])
            );
        }

        // ── 7. Create Render Pass ─────────────────────────────────────────────────
        _render_pass = std::make_unique<vulkan_render_pass_t>(_device.get(), _swapchain->format());

        // ── 8. Create Graphics Pipeline ───────────────────────────────────────────
        _graphics_pipeline = std::make_unique<vulkan_pipeline_t>(_device.get(), _render_pass->vk_render_pass());

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
        _frame_active = false;
        _render_pass_active = false;
        _skip_frame = true;

        vkDeviceWaitIdle(_device->vk_device());

        _framebuffers = framebuffer_array_t{ _device->vk_device() };

        if (_pending_resize_width > 0 && _pending_resize_height > 0)
            _swapchain->resize(_pending_resize_width, _pending_resize_height);
        else
            _swapchain->recreate();

        recreate_render_finished_semaphores();

        _framebuffers = _swapchain->create_framebuffers(_render_pass->vk_render_pass());

        LOG_GRAPHICS_INFO("Swapchain & framebuffers recreated after resize");
    }

    void vulkan_rhi_context_t::recreate_render_finished_semaphores()
    {
        for (const VkSemaphore& sem: _render_finished_semaphores)
        {
            if (sem)
                vkDestroySemaphore(_device->vk_device(), sem, nullptr);
        }
        _render_finished_semaphores.clear();

        VkSemaphoreCreateInfo sem_info{ };
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        const uint32_t image_count{ _swapchain->get_image_count() };
        _render_finished_semaphores.resize(image_count);

        for (uint32_t i{ 0 }; i < image_count; ++i)
        {
            VK_CHECK_FATAL(
                vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &_render_finished_semaphores[i])
            );
        }
    }

    uint32_t vulkan_rhi_context_t::find_memory_type(const uint32_t type_filter,
                                                    const VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties mem_properties{ };
        vkGetPhysicalDeviceMemoryProperties(_device->physical_device(), &mem_properties);

        for (uint32_t i{ 0 }; i < mem_properties.memoryTypeCount; ++i)
        {
            const bool type_matches{ (type_filter & (1u << i)) != 0 };
            const bool props_match{ (mem_properties.memoryTypes[i].propertyFlags & properties) == properties };

            if (type_matches && props_match)
                return i;
        }

        LOG_GRAPHICS_FATAL("Failed to find suitable Vulkan memory type");
        return 0;
    }

    VkCommandBuffer vulkan_rhi_context_t::begin_single_time_commands() const
    {
        VkCommandBufferAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = _command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd{ VK_NULL_HANDLE };
        vkAllocateCommandBuffers(_device->vk_device(), &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info{ };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
    }

    void vulkan_rhi_context_t::end_single_time_commands(VkCommandBuffer cmd) const
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info{ };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VK_CHECK_FATAL(vkQueueSubmit(_device->graphics_queue(), 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK_FATAL(vkQueueWaitIdle(_device->graphics_queue()));

        vkFreeCommandBuffers(_device->vk_device(), _command_pool, 1, &cmd);
    }
} // namespace carrot::rhi::vulkan
