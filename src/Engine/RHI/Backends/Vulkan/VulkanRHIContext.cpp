//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanRHIContext.h"

#include "HotReload/ShaderWatcher.h"
#include "Pipelines/VulkanTexturedQuadPipeline.h"
#include "RHI/SamplerPresets.h"
#include "Renderer/Draw/TexturedQuadCameraUniform.h"
#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanSampler.h"
#include "VulkanTexture.h"
#include "Window/Window.h"

#include <algorithm>

namespace carrot::rhi::vulkan {
    namespace {
        [[nodiscard]] uint64_t current_time_ms() noexcept
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        [[nodiscard]] uint64_t elapsed_ms(const uint64_t start_ms) noexcept
        {
            return current_time_ms() - start_ms;
        }

        [[nodiscard]] VkBufferUsageFlags to_vk_buffer_usage(const buffer_usage_t usage) noexcept
        {
            using enum buffer_usage_t;

            switch (usage)
            {
                case vertex:
                    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                case index:
                    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                case uniform:
                    return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                case staging:
                    return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                default:
                    return 0;
            }
        }

        [[nodiscard]] VkFilter to_vk_filter(const sampler_filter_t filter) noexcept
        {
            switch (filter)
            {
                case sampler_filter_t::nearest: return VK_FILTER_NEAREST;
                case sampler_filter_t::linear: return VK_FILTER_LINEAR;
                default: return VK_FILTER_NEAREST;
            }
        }

        [[nodiscard]] VkSamplerMipmapMode to_vk_mip_filter(const sampler_mip_filter_t filter) noexcept
        {
            switch (filter)
            {
                case sampler_mip_filter_t::nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
                case sampler_mip_filter_t::linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
                default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            }
        }

        [[nodiscard]] VkSamplerAddressMode to_vk_address_mode(const sampler_address_mode_t mode) noexcept
        {
            switch (mode)
            {
                case sampler_address_mode_t::clamp_to_edge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case sampler_address_mode_t::repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case sampler_address_mode_t::mirrored_repeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
        }

        // Stage-aware recording means one frame may consume descriptor sets across
        // multiple stage submissions before the command buffer is submitted.
        constexpr uint32_t k_initial_textured_quad_descriptor_sets_per_frame{ 256 };

        constexpr uint32_t k_max_textured_quad_descriptor_sets{
            k_max_frames_in_flight * k_initial_textured_quad_descriptor_sets_per_frame
        };

        constexpr uint64_t k_swapchain_recreate_settle_delay_ms{ 100 };
    } // anonymous namespace

    // PUBLIC
    vulkan_rhi_context_t::vulkan_rhi_context_t(const rhi_desc_t& desc)
    {
        init(desc);
    }

    vulkan_rhi_context_t::~vulkan_rhi_context_t()
    {
        if (!_device)
        {
            if (_vk_surface)
            {
                vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
                _vk_surface = VK_NULL_HANDLE;
            }

            if (_vk_instance)
            {
                vkDestroyInstance(_vk_instance, nullptr);
                _vk_instance = VK_NULL_HANDLE;
            }

            return;
        }

        VkDevice device{ _device->vk_device() };
        vkDeviceWaitIdle(device);

        _framebuffers = { };
        destroy_descriptor_pool();

        release_textured_quad_resources();

        _sampler_cache.clear();
        _textured_quad_pipeline.reset();
        _render_pass.reset();
        destroy_all_auxiliary_surfaces();

        if (_swapchain)
        {
            for (uint32_t i{ 0 }; i < _swapchain->get_image_count(); ++i)
            {
                if (_render_finished_semaphores[i])
                    vkDestroySemaphore(device, _render_finished_semaphores[i], nullptr);
            }

            _swapchain.reset();
        }

        for (const auto& frame: _frames)
        {
            if (frame.image_acquire)
                vkDestroySemaphore(device, frame.image_acquire, nullptr);

            if (frame.in_flight)
                vkDestroyFence(device, frame.in_flight, nullptr);

            if (_command_pool != VK_NULL_HANDLE && frame.command_buffer)
                vkFreeCommandBuffers(device, _command_pool, 1, &frame.command_buffer);
        }

        if (_command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, _command_pool, nullptr);
            _command_pool = VK_NULL_HANDLE;
        }

        _graphics_queue.reset();
        _device.reset();

        if (_vk_surface)
        {
            vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
            _vk_surface = VK_NULL_HANDLE;
        }

        if (_vk_instance)
        {
            vkDestroyInstance(_vk_instance, nullptr);
            _vk_instance = VK_NULL_HANDLE;
        }
    }

    void vulkan_rhi_context_t::begin_frame()
    {
        _skip_frame = false;
        _frame_active = false;
        _render_pass_active = false;
        _recorded_stages.clear();
        _textured_quad_descriptor_set_cursor[_current_frame] = 0;
        sync_auxiliary_surface_sizes();

        if (_swapchain_dirty)
        {
            const uint64_t now_ms{ current_time_ms() };
            const bool resize_settling{
                _last_resize_request_time_ms != 0 &&
                (now_ms - _last_resize_request_time_ms) < k_swapchain_recreate_settle_delay_ms
            };

            if (window::is_resizing(_presentation_window_id) || resize_settling)
            {
                LOG_GRAPHICS_INFO("Deferring main swapchain recreation: resizing={} settling={} pending={}x{}",
                                  window::is_resizing(_presentation_window_id),
                                  resize_settling,
                                  _pending_resize_width,
                                  _pending_resize_height);
                _skip_frame = true;
                return;
            }

            LOG_GRAPHICS_INFO("Recreating main swapchain resources at begin_frame with pending size {}x{}",
                              _pending_resize_width,
                              _pending_resize_height);
            recreate_swapchain_dependent_resources();
            _swapchain_dirty = false;
            _skip_frame = true;
            return;
        }

        const frame_resources_t& frame{ _frames[_current_frame] };

        // Wait for the previous use of this frame to finish
        const uint64_t wait_start_ms{ current_time_ms() };
        VK_CHECK_FATAL(vkWaitForFences(_device->vk_device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));
        LOG_GRAPHICS_INFO("vkWaitForFences(begin_frame frame={}) completed in {} ms",
                          _current_frame,
                          elapsed_ms(wait_start_ms));

        // Hot-reload check: safe here because previous frames are done
        if (_pending_pipeline_reload)
        {
            LOG_GRAPHICS_INFO("Safe reload point reached — destroying old pipeline");
            _textured_quad_pipeline.reset();
            LOG_GRAPHICS_INFO("Old pipeline destroyed");

            _textured_quad_pipeline = std::make_unique<vulkan_textured_quad_pipeline_t>(
                _device.get(), _render_pass->vk_render_pass(), _shader_files);
            _pending_pipeline_reload = false;
            LOG_GRAPHICS_INFO("Pipeline hot-reloaded (safe point)");
        }

        // Acquire next swapchain image
        const VkResult acquire_result{
            // Non-blocking acquire keeps the app responsive during Wayland fullscreen/configure transitions.
            vkAcquireNextImageKHR(_device->vk_device(), _swapchain->vk_swapchain(), 0, frame.image_acquire,
                                  VK_NULL_HANDLE, &_current_image_index)
        };

        if (acquire_result == VK_NOT_READY)
        {
            _skip_frame = true;
            return;
        }

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

    void vulkan_rhi_context_t::record_textured_quad_stage(const textured_quad_stage_record_t& stage)
    {
        if (_skip_frame || !_frame_active)
            return;

        uint32_t batch_count{ 0 };
        const uint32_t descriptor_set_offset{ prepare_textured_quad_stage_descriptors(stage, batch_count) };
        if (batch_count == 0)
            return;

        recorded_stage_t recorded_stage{ };
        recorded_stage.stage = stage;
        recorded_stage.descriptor_set_offset = descriptor_set_offset;
        recorded_stage.descriptor_set_count = batch_count;
        _recorded_stages.push_back(recorded_stage);

        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
        {
            encode_textured_quad_stage_to_command_buffer(_frames[_current_frame].command_buffer,
                                                         stage,
                                                         descriptor_set_offset,
                                                         batch_count);
        }
    }

    void vulkan_rhi_context_t::end_frame()
    {
        if (_skip_frame || !_frame_active)
            return;

        const frame_resources_t& frame{ _frames[_current_frame] };
        std::vector<auxiliary_surface_t*> acquired_aux_surfaces;
        // Engine policy: when the main presentation surface is fullscreen, auxiliary windows are not
        // visible in typical gameplay mode, so skip auxiliary rendering/presentation work.
        const bool main_window_resizing{ window::is_resizing(_presentation_window_id) };
        const bool allow_auxiliary_presentation{
            !window::is_fullscreen(_presentation_window_id) &&
            !main_window_resizing &&
            !_swapchain_dirty
        };

        // 1. End render pass (if not already ended in record_frame)
        if (_render_pass_active)
        {
            vkCmdEndRenderPass(frame.command_buffer);
            _render_pass_active = false;
        }

        for (auxiliary_surface_t& aux_surface : _auxiliary_surfaces)
        {
            if (!allow_auxiliary_presentation)
                break;

            if (!window::has_window(aux_surface.id))
                continue;

            if (window::is_resizing(aux_surface.id) || aux_surface.swapchain_dirty)
                continue;

            const VkResult acquire_result{
                // Non-blocking acquire prevents an auxiliary surface from stalling the whole frame.
                vkAcquireNextImageKHR(_device->vk_device(),
                                      aux_surface.swapchain->vk_swapchain(),
                                      0,
                                      aux_surface.image_acquire[_current_frame],
                                      VK_NULL_HANDLE,
                                      &aux_surface.current_image_index)
            };

            if (acquire_result == VK_NOT_READY)
                continue;

            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
            {
                aux_surface.swapchain_dirty = true;
                continue;
            }

            if (acquire_result != VK_SUCCESS)
            {
                VK_CHECK_FATAL(acquire_result);
                continue;
            }

            constexpr VkClearValue clear_color{ { { 0.02f, 0.02f, 0.04f, 1.0f } } };

            VkRenderPassBeginInfo rp_begin{ };
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.renderPass = _render_pass->vk_render_pass();
            rp_begin.framebuffer = aux_surface.framebuffers[aux_surface.current_image_index];
            rp_begin.renderArea.offset = { 0, 0 };
            rp_begin.renderArea.extent = aux_surface.swapchain->extent();
            rp_begin.clearValueCount = 1;
            rp_begin.pClearValues = &clear_color;

            vkCmdBeginRenderPass(frame.command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
            for (const recorded_stage_t& recorded_stage : _recorded_stages)
            {
                if (!presentation_mask_includes(recorded_stage.stage.presentation_mask,
                                                aux_surface.presentation_channel_mask))
                    continue;
                encode_textured_quad_stage_to_command_buffer(frame.command_buffer,
                                                             recorded_stage.stage,
                                                             recorded_stage.descriptor_set_offset,
                                                             recorded_stage.descriptor_set_count);
            }
            vkCmdEndRenderPass(frame.command_buffer);

            acquired_aux_surfaces.push_back(&aux_surface);
        }

        // 2. End command buffer
        VK_CHECK_FATAL(vkEndCommandBuffer(frame.command_buffer));

        // 3. Submit to graphics queue
        std::vector<VkSemaphore> wait_semaphores{ frame.image_acquire };
        std::vector<VkPipelineStageFlags> wait_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        std::vector<VkSemaphore> signal_semaphores{ _render_finished_semaphores[_current_image_index] };

        for (auxiliary_surface_t* aux_surface : acquired_aux_surfaces)
        {
            wait_semaphores.push_back(aux_surface->image_acquire[_current_frame]);
            wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            signal_semaphores.push_back(aux_surface->render_finished[_current_frame]);
        }

        VkSubmitInfo submit_info{ };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = wait_stages.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &frame.command_buffer;
        submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
        submit_info.pSignalSemaphores = signal_semaphores.data();

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

        const VkResult present_result{ vkQueuePresentKHR(_device->graphics_queue(), &present_info) };
        bool skip_auxiliary_present_for_main_resize{ false };
        LOG_GRAPHICS_INFO("Main vkQueuePresentKHR returned {} for image {} on frame {}",
                          static_cast<int>(present_result),
                          _current_image_index,
                          _current_frame);

        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
        {
            _last_resize_request_time_ms = current_time_ms();
            _swapchain_dirty = true;
            skip_auxiliary_present_for_main_resize = true;
            LOG_GRAPHICS_WARN("Present suboptimal/out of date");
        }
        else if (present_result != VK_SUCCESS)
        {
            VK_CHECK_FATAL(present_result);
        }

        if (skip_auxiliary_present_for_main_resize)
        {
            _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
            _frame_active = false;
            return;
        }

        for (auxiliary_surface_t* aux_surface : acquired_aux_surfaces)
        {
            VkSwapchainKHR aux_swapchain{ aux_surface->swapchain->vk_swapchain() };
            const uint32_t aux_image_index{ aux_surface->current_image_index };
            const VkSemaphore aux_wait{ aux_surface->render_finished[_current_frame] };

            VkPresentInfoKHR aux_present_info{ };
            aux_present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            aux_present_info.waitSemaphoreCount = 1;
            aux_present_info.pWaitSemaphores = &aux_wait;
            aux_present_info.swapchainCount = 1;
            aux_present_info.pSwapchains = &aux_swapchain;
            aux_present_info.pImageIndices = &aux_image_index;

            const VkResult aux_present_result{ vkQueuePresentKHR(_device->graphics_queue(), &aux_present_info) };
            LOG_GRAPHICS_INFO("Aux vkQueuePresentKHR returned {} for window {} image {} on frame {}",
                              static_cast<int>(aux_present_result),
                              static_cast<unsigned long long>(aux_surface->id),
                              aux_image_index,
                              _current_frame);
            if (aux_present_result == VK_ERROR_OUT_OF_DATE_KHR || aux_present_result == VK_SUBOPTIMAL_KHR)
            {
                aux_surface->swapchain_dirty = true;
            }
            else if (aux_present_result != VK_SUCCESS)
            {
                VK_CHECK_FATAL(aux_present_result);
            }
        }

        // Advance frame index
        _current_frame = (_current_frame + 1) % k_max_frames_in_flight;
        _frame_active = false;
    }

    void vulkan_rhi_context_t::release_asset_references()
    {
        if (!_device)
            return;

        VK_CHECK_FATAL(vkDeviceWaitIdle(_device->vk_device()));

        for (auto& frame_buffers: _textured_quad.camera_uniform_buffers)
            for (auto& buffer: frame_buffers)
                buffer.reset();

        for (auto& frame_descriptor_sets: _textured_quad.camera_descriptor_sets)
            frame_descriptor_sets.fill(VK_NULL_HANDLE);

        for (auto& frame_sets: _textured_quad.descriptor_sets)
            frame_sets.clear();

        destroy_descriptor_pool();
    }

    void vulkan_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        _last_resize_request_time_ms = current_time_ms();
        LOG_GRAPHICS_INFO("Main presentation resize requested to {}x{}", width, height);

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

    bool vulkan_rhi_context_t::add_presentation_window(const window::window_id_t window_id,
                                                       const uint32_t presentation_channel_mask)
    {
        if (window_id == window::invalid_window_id || window_id == _presentation_window_id)
            return false;

        const bool already_registered{
            std::find_if(_auxiliary_surfaces.begin(),
                         _auxiliary_surfaces.end(),
                         [window_id](const auxiliary_surface_t& surface) { return surface.id == window_id; }) != _auxiliary_surfaces.end()
        };
        if (already_registered)
            return true;

        return create_auxiliary_surface(window_id, presentation_channel_mask);
    }

    bool vulkan_rhi_context_t::remove_presentation_window(const window::window_id_t window_id)
    {
        if (window_id == window::invalid_window_id)
            return false;

        auto it{
            std::find_if(_auxiliary_surfaces.begin(),
                         _auxiliary_surfaces.end(),
                         [window_id](const auxiliary_surface_t& surface) { return surface.id == window_id; })
        };
        if (it == _auxiliary_surfaces.end())
            return false;

        // The auxiliary surface resources (semaphores/framebuffers/swapchain images) may still be
        // referenced by in-flight submissions. Wait before destroying to satisfy Vulkan lifetime rules.
        wait_idle();
        destroy_auxiliary_surface(*it);
        _auxiliary_surfaces.erase(it);
        return true;
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
            VK_CHECK_FATAL(vkDeviceWaitIdle(device));
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

        // staging resources are no longer needed after upload
        vkFreeMemory(device, staging_memory, nullptr);
        vkDestroyBuffer(device, staging_buffer, nullptr);

        std::unique_ptr<vulkan_texture_t> texture{ std::make_unique<vulkan_texture_t>(_device.get()) };
        texture->set_width(info.width);
        texture->set_height(info.height);
        texture->set_format(info.format);
        texture->set_image(image);
        texture->set_memory(image_memory);
        texture->set_view(image_view);

        LOG_GRAPHICS_INFO("Created Vulkan texture: {}x{}", info.width, info.height);

        return texture;
    }

    std::unique_ptr<rhi_buffer_t> vulkan_rhi_context_t::create_buffer(const buffer_create_info_t& info)
    {
        if (info.size_bytes == 0)
        {
            LOG_GRAPHICS_ERROR("create_buffer failed: size_bytes was 0");
            return nullptr;
        }

        VkDevice device{ _device->vk_device() };
        const VkDeviceSize size{ static_cast<VkDeviceSize>(info.size_bytes) };
        const VkBufferUsageFlags base_usage{ to_vk_buffer_usage(info.usage) };

        // Explicit CPU-writable path
        if (info.cpu_writable || info.usage == buffer_usage_t::staging)
        {
            VkBuffer buffer{ VK_NULL_HANDLE };
            VkDeviceMemory memory{ VK_NULL_HANDLE };

            constexpr VkMemoryPropertyFlags memory_properties{
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };

            create_vk_buffer(size, base_usage, memory_properties, buffer, memory);

            if (info.initial_data != nullptr)
            {
                void* mapped{ nullptr };
                VK_CHECK_FATAL(vkMapMemory(device, memory, 0, size, 0, &mapped));
                std::memcpy(mapped, info.initial_data, info.size_bytes);
                vkUnmapMemory(device, memory);
            }

            auto result{ std::make_unique<vulkan_buffer_t>(device, info.size_bytes, info.usage) };
            result->set_buffer(buffer);
            result->set_memory(memory);
            result->set_memory_properties(memory_properties);

            return result;
        }

        // Static GPU-local path
        VkBufferUsageFlags dst_usage{ base_usage };
        if (info.initial_data != nullptr)
            dst_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VkBuffer dst_buffer{ VK_NULL_HANDLE };
        VkDeviceMemory dst_memory{ VK_NULL_HANDLE };

        constexpr VkMemoryPropertyFlags dst_memory_properties{ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

        create_vk_buffer(size, dst_usage, dst_memory_properties, dst_buffer, dst_memory);

        if (info.initial_data != nullptr)
        {
            VkBuffer staging_buffer{ VK_NULL_HANDLE };
            VkDeviceMemory staging_memory{ VK_NULL_HANDLE };

            constexpr VkMemoryPropertyFlags staging_memory_properties{
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };

            create_vk_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_memory_properties, staging_buffer,
                             staging_memory);

            void* mapped{ nullptr };
            VK_CHECK_FATAL(vkMapMemory(device, staging_memory, 0, size, 0, &mapped));
            std::memcpy(mapped, info.initial_data, info.size_bytes);
            vkUnmapMemory(device, staging_memory);

            copy_buffer(staging_buffer, dst_buffer, size);
            VK_CHECK_FATAL(vkDeviceWaitIdle(device));

            vkDestroyBuffer(device, staging_buffer, nullptr);
            vkFreeMemory(device, staging_memory, nullptr);
        }

        auto result{ std::make_unique<vulkan_buffer_t>(device, info.size_bytes, info.usage) };
        result->set_buffer(dst_buffer);
        result->set_memory(dst_memory);
        result->set_memory_properties(dst_memory_properties);

        return result;
    }

    std::unique_ptr<rhi_sampler_t> vulkan_rhi_context_t::create_sampler(const sampler_desc_t& desc) const
    {
        if (!_device)
            return nullptr;

        VkSamplerCreateInfo sampler_info{ };
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = to_vk_filter(desc.mag_filter);
        sampler_info.minFilter = to_vk_filter(desc.min_filter);
        sampler_info.mipmapMode = to_vk_mip_filter(desc.mip_filter);
        sampler_info.addressModeU = to_vk_address_mode(desc.address_u);
        sampler_info.addressModeV = to_vk_address_mode(desc.address_v);
        sampler_info.addressModeW = to_vk_address_mode(desc.address_w);
        sampler_info.mipLodBias = desc.mip_lod_bias;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1.0f;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.minLod = desc.min_lod;
        sampler_info.maxLod = desc.max_lod;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler{ VK_NULL_HANDLE };
        if (vkCreateSampler(_device->vk_device(), &sampler_info, nullptr, &sampler) != VK_SUCCESS)
        {
            LOG_GRAPHICS_ERROR("Failed to create Vulkan sampler");
            return nullptr;
        }

        return std::make_unique<vulkan_sampler_t>(_device->vk_device(), sampler, desc);
    }

    rhi_sampler_t* vulkan_rhi_context_t::get_or_create_sampler(const sampler_desc_t& desc)
    {
        if (const auto it{ _sampler_cache.find(desc) }; it != _sampler_cache.end())
            return it->second.get();

        std::unique_ptr<rhi_sampler_t> sampler{ create_sampler(desc) };
        if (!sampler)
            return nullptr;

        rhi_sampler_t* result{ sampler.get() };
        _sampler_cache.emplace(desc, std::move(sampler));

        return result;
    }

    void vulkan_rhi_context_t::wait_idle()
    {
        if (_device)
            VK_CHECK_FATAL(vkDeviceWaitIdle(_device->vk_device()));
    }

    bool vulkan_rhi_context_t::create_surface_for_window(const window::window_id_t window_id, VkSurfaceKHR& out_surface) const
    {
        if (!window::has_window(window_id))
            return false;

        const core::platform::native_window_handle_t handle{ window::get_native_handle(window_id) };

#if defined(CARROT_PLATFORM_WAYLAND)
        VkWaylandSurfaceCreateInfoKHR surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surf_info.display = handle.wayland_t.display;
        surf_info.surface = handle.wayland_t.surface;
        return vkCreateWaylandSurfaceKHR(_vk_instance, &surf_info, nullptr, &out_surface) == VK_SUCCESS;
#elif defined(CARROT_PLATFORM_WIN32)
        VkWin32SurfaceCreateInfoKHR surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surf_info.hwnd = static_cast<HWND>(handle.win32_t.hwnd);
        surf_info.hinstance = static_cast<HINSTANCE>(handle.win32_t.hinstance);
        return vkCreateWin32SurfaceKHR(_vk_instance, &surf_info, nullptr, &out_surface) == VK_SUCCESS;
#elif defined(CARROT_PLATFORM_COCOA)
        VkMetalSurfaceCreateInfoEXT surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        surf_info.pLayer = handle.cocoa_t.metal_layer;
        return vkCreateMetalSurfaceEXT(_vk_instance, &surf_info, nullptr, &out_surface) == VK_SUCCESS;
#else
        (void)window_id;
        (void)out_surface;
        return false;
#endif
    }

    bool vulkan_rhi_context_t::create_auxiliary_surface(const window::window_id_t window_id,
                                                        const uint32_t presentation_channel_mask)
    {
        if (!_device || !_render_pass || !_vk_instance)
            return false;

        auxiliary_surface_t surface{ };
        surface.id = window_id;
        surface.presentation_channel_mask = presentation_channel_mask;
        surface.framebuffers = framebuffer_array_t{ _device->vk_device() };

        if (!create_surface_for_window(window_id, surface.surface) || surface.surface == VK_NULL_HANDLE)
            return false;

        const uint32_t width{ window::get_width(window_id) };
        const uint32_t height{ window::get_height(window_id) };
        if (width == 0 || height == 0)
        {
            vkDestroySurfaceKHR(_vk_instance, surface.surface, nullptr);
            return false;
        }

        surface.swapchain = std::make_unique<vulkan_swapchain_t>(_device.get(), surface.surface, width, height);
        surface.framebuffers = surface.swapchain->create_framebuffers(_render_pass->vk_render_pass());
        surface.last_width = width;
        surface.last_height = height;

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            VkSemaphoreCreateInfo sem_info{ };
            sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &surface.image_acquire[i]));
            VK_CHECK_FATAL(vkCreateSemaphore(_device->vk_device(), &sem_info, nullptr, &surface.render_finished[i]));
        }

        _auxiliary_surfaces.push_back(std::move(surface));
        return true;
    }

    void vulkan_rhi_context_t::destroy_auxiliary_surface(auxiliary_surface_t& surface) noexcept
    {
        if (!_device)
            return;

        const VkDevice device{ _device->vk_device() };

        for (VkSemaphore& sem : surface.image_acquire)
        {
            if (sem)
            {
                vkDestroySemaphore(device, sem, nullptr);
                sem = VK_NULL_HANDLE;
            }
        }

        for (VkSemaphore& sem : surface.render_finished)
        {
            if (sem)
            {
                vkDestroySemaphore(device, sem, nullptr);
                sem = VK_NULL_HANDLE;
            }
        }

        surface.framebuffers = framebuffer_array_t{ device };
        surface.swapchain.reset();

        if (surface.surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(_vk_instance, surface.surface, nullptr);
            surface.surface = VK_NULL_HANDLE;
        }
    }

    void vulkan_rhi_context_t::destroy_all_auxiliary_surfaces() noexcept
    {
        for (auxiliary_surface_t& surface : _auxiliary_surfaces)
            destroy_auxiliary_surface(surface);
        _auxiliary_surfaces.clear();
    }

    void vulkan_rhi_context_t::sync_auxiliary_surface_sizes()
    {
        if (!_render_pass)
            return;

        bool needs_idle_for_destroy{ false };
        for (auto it = _auxiliary_surfaces.begin(); it != _auxiliary_surfaces.end();)
        {
            if (!window::has_window(it->id))
            {
                if (!needs_idle_for_destroy)
                {
                    // Closed windows can disappear between frames while previous submissions still
                    // reference their sync/framebuffer objects.
                    wait_idle();
                    needs_idle_for_destroy = true;
                }
                destroy_auxiliary_surface(*it);
                it = _auxiliary_surfaces.erase(it);
                continue;
            }

            const uint32_t width{ window::get_width(it->id) };
            const uint32_t height{ window::get_height(it->id) };
            const bool resizing{ window::is_resizing(it->id) };
            const bool size_changed{ width > 0 && height > 0 && (width != it->last_width || height != it->last_height) };

            if (resizing)
            {
                if (size_changed)
                    it->swapchain_dirty = true;
                ++it;
                continue;
            }

            if (width > 0 && height > 0 && (size_changed || it->swapchain_dirty))
            {
                if (size_changed)
                    it->swapchain->resize(width, height);
                else
                    it->swapchain->recreate();

                it->framebuffers = it->swapchain->create_framebuffers(_render_pass->vk_render_pass());
                it->last_width = width;
                it->last_height = height;
                it->swapchain_dirty = false;
            }

            ++it;
        }
    }

    uint32_t vulkan_rhi_context_t::prepare_textured_quad_stage_descriptors(const textured_quad_stage_record_t& stage,
                                                                           uint32_t& out_batch_count)
    {
        out_batch_count = 0;

        if (_textured_quad_pipeline == nullptr)
        {
            LOG_GRAPHICS_FATAL("Textured quad pipeline not initialized");
            return 0;
        }

        if (stage.batches.empty())
            return 0;

        if (stage.stage_slot >= k_max_textured_quad_stage_records_per_frame)
        {
            LOG_GRAPHICS_FATAL("Vulkan textured quad stage slot {} exceeds max supported stage slots {}", stage.stage_slot,
                               k_max_textured_quad_stage_records_per_frame);
            return 0;
        }

        renderer::textured_quad_camera_uniform_t camera_uniform{ };
        camera_uniform.view_projection = stage.view_projection;
        if (!_textured_quad.camera_uniform_buffers[_current_frame][stage.stage_slot] ||
            !_textured_quad.camera_uniform_buffers[_current_frame][stage.stage_slot]->write(&camera_uniform,
                                                                                            sizeof(camera_uniform), 0))
        {
            LOG_GRAPHICS_FATAL("Failed to upload Vulkan textured quad camera uniform");
            return 0;
        }
        write_textured_quad_camera_descriptor_set(_current_frame, stage.stage_slot);

        const uint32_t batch_count{ static_cast<uint32_t>(stage.batches.size()) };
        const uint32_t descriptor_set_offset{ _textured_quad_descriptor_set_cursor[_current_frame] };
        ensure_textured_quad_descriptor_sets_for_frame(_current_frame, descriptor_set_offset + batch_count);

        const std::vector<VkDescriptorSet_T*>& frame_descriptor_sets{ _textured_quad.descriptor_sets[_current_frame] };
        if (frame_descriptor_sets.size() < descriptor_set_offset + batch_count)
        {
            LOG_GRAPHICS_FATAL("Textured quad descriptor set allocation shortfall for frame {}: required {}, got {}",
                               _current_frame,
                               descriptor_set_offset + batch_count,
                               frame_descriptor_sets.size());
            return 0;
        }

        for (uint32_t batch_index{ 0 }; batch_index < batch_count; ++batch_index)
        {
            const renderer::textured_quad_batch_t& batch{ stage.batches[batch_index] };
            if (batch.texture == nullptr || batch.index_count == 0)
                continue;

            const sampler_desc_t sampler_desc{ sampler_desc_from_preset(batch.sampler_preset) };
            const rhi_sampler_t* sampler{ get_or_create_sampler(sampler_desc) };
            if (!sampler)
            {
                LOG_GRAPHICS_WARN("Failed to resolve Vulkan sampler for textured quad batch");
                continue;
            }

            VkDescriptorSet descriptor_set{ frame_descriptor_sets[descriptor_set_offset + batch_index] };
            write_textured_quad_descriptor_set(descriptor_set, *batch.texture, *sampler);
        }

        _textured_quad_descriptor_set_cursor[_current_frame] += batch_count;
        out_batch_count = batch_count;
        return descriptor_set_offset;
    }

    void vulkan_rhi_context_t::encode_textured_quad_stage_to_command_buffer(const VkCommandBuffer command_buffer,
                                                                            const textured_quad_stage_record_t& stage,
                                                                            const uint32_t descriptor_set_offset,
                                                                            const uint32_t batch_count)
    {
        if (_textured_quad_pipeline == nullptr)
        {
            LOG_GRAPHICS_FATAL("Textured quad pipeline not initialized");
            return;
        }

        const vulkan_buffer_t* vertex_buffer{ dynamic_cast<const vulkan_buffer_t*>(stage.vertex_buffer) };
        const vulkan_buffer_t* index_buffer{ dynamic_cast<const vulkan_buffer_t*>(stage.index_buffer) };
        if (vertex_buffer == nullptr || index_buffer == nullptr)
        {
            LOG_GRAPHICS_FATAL("Textured quad geometry not initialized");
            return;
        }

        if (batch_count == 0)
            return;

        if (stage.stage_slot >= k_max_textured_quad_stage_records_per_frame)
        {
            LOG_GRAPHICS_FATAL("Vulkan textured quad stage slot {} exceeds max supported stage slots {}", stage.stage_slot,
                               k_max_textured_quad_stage_records_per_frame);
            return;
        }

        const std::vector<VkDescriptorSet_T*>& frame_descriptor_sets{ _textured_quad.descriptor_sets[_current_frame] };
        if (frame_descriptor_sets.size() < descriptor_set_offset + batch_count)
        {
            LOG_GRAPHICS_FATAL("Textured quad descriptor range out of bounds for frame {}: requested [{}, {}) with {} sets",
                               _current_frame,
                               descriptor_set_offset,
                               descriptor_set_offset + batch_count,
                               frame_descriptor_sets.size());
            return;
        }

        const chlm::uint_rect viewport_rect{ stage.viewport.rect_px };

        VkViewport viewport{ };
        viewport.x = static_cast<float>(viewport_rect.position.x);
        viewport.y = static_cast<float>(viewport_rect.position.y);
        viewport.width = static_cast<float>(viewport_rect.size.x);
        viewport.height = static_cast<float>(viewport_rect.size.y);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor{ };
        scissor.offset = {
            static_cast<int32_t>(viewport_rect.position.x),
            static_cast<int32_t>(viewport_rect.position.y)
        };
        scissor.extent = {
            viewport_rect.size.x,
            viewport_rect.size.y
        };

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            _textured_quad_pipeline->vk_pipeline()
        );

        const VkBuffer vertex_buffers[]{ vertex_buffer->vk_buffer() };
        const VkDeviceSize offsets[]{ 0 };

        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffer, index_buffer->vk_buffer(), 0, VK_INDEX_TYPE_UINT32);

        VkDescriptorSet camera_descriptor_set{ _textured_quad.camera_descriptor_sets[_current_frame][stage.stage_slot] };

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                _textured_quad_pipeline->vk_layout(), 0, 1, &camera_descriptor_set, 0, nullptr);

        for (uint32_t batch_index{ 0 }; batch_index < batch_count; ++batch_index)
        {
            const renderer::textured_quad_batch_t& batch{ stage.batches[batch_index] };

            if (batch.texture == nullptr || batch.index_count == 0)
                continue;

            VkDescriptorSet descriptor_set{ frame_descriptor_sets[descriptor_set_offset + batch_index] };

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    _textured_quad_pipeline->vk_layout(), 1, 1, &descriptor_set, 0, nullptr);

            vkCmdDrawIndexed(command_buffer, batch.index_count, 1, batch.first_index, 0, 0);
        }
    }

    // PRIVATE
    void vulkan_rhi_context_t::init(const rhi_desc_t& desc)
    {
        const window::window_id_t presentation_window_id{
            window::has_window(desc.presentation_window_id)
                ? desc.presentation_window_id
                : window::get_main_window_id()
        };
        _presentation_window_id = presentation_window_id;
        core::platform::native_window_handle_t handle{ window::get_native_handle(presentation_window_id) };
        _shader_files = desc.shader_files;

        // ── 1. Create Vulkan Instance ─────────────────────────────────────────────
        std::vector<const char*> instance_extensions{
            VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(CARROT_PLATFORM_WAYLAND)
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif defined(CARROT_PLATFORM_WIN32)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(CARROT_PLATFORM_COCOA)
            VK_EXT_METAL_SURFACE_EXTENSION_NAME,
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
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

#if defined(CARROT_PLATFORM_COCOA)
        inst_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

#ifdef _DEBUG
        if (desc.enable_debug_layers)
        {
            const char* validation_layer{ "VK_LAYER_KHRONOS_validation" };
            inst_info.enabledLayerCount = 1;
            inst_info.ppEnabledLayerNames = &validation_layer;
        }
#endif

        VK_CHECK_FATAL(vkCreateInstance(&inst_info, nullptr, &_vk_instance));

        // ── 2. Create Platform Specific Surface ───────────────────────────────────
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
        VK_CHECK_FATAL(vkCreateWin32SurfaceKHR(_vk_instance, &surf_info, nullptr, &_vk_surface));
#elif defined(CARROT_PLATFORM_COCOA)
        VkMetalSurfaceCreateInfoEXT surf_info{ };
        surf_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        surf_info.pLayer = handle.cocoa_t.metal_layer;
        VK_CHECK_FATAL(vkCreateMetalSurfaceEXT(_vk_instance, &surf_info, nullptr, &_vk_surface));
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

        std::vector<const char*> device_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

#if defined(CARROT_PLATFORM_COCOA)
        device_extensions.push_back("VK_KHR_portability_subset");
#endif

        VkDeviceCreateInfo device_info{ };
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        device_info.ppEnabledExtensionNames = device_extensions.data();

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

        // ── 8. Create Graphics Pipelines ──────────────────────────────────────────
        _textured_quad_pipeline = std::make_unique<vulkan_textured_quad_pipeline_t>(
            _device.get(), _render_pass->vk_render_pass(), _shader_files);

        create_descriptor_pool();

        for (uint32_t frame_index{ 0 }; frame_index < k_max_frames_in_flight; ++frame_index)
            for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_records_per_frame; ++stage_slot)
                _textured_quad.camera_uniform_buffers[frame_index][stage_slot] = create_buffer({
                    .size_bytes = sizeof(renderer::textured_quad_camera_uniform_t),
                    .usage = buffer_usage_t::uniform,
                    .cpu_writable = true
                });

        allocate_textured_quad_camera_descriptor_sets();

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
        const uint64_t recreate_start_ms{ current_time_ms() };
        LOG_GRAPHICS_INFO("Begin main swapchain recreation: pending={}x{} current={}x{}",
                          _pending_resize_width,
                          _pending_resize_height,
                          _swapchain ? _swapchain->get_width() : 0,
                          _swapchain ? _swapchain->get_height() : 0);

        if (window::should_close(_presentation_window_id))
        {
            LOG_GRAPHICS_INFO("Skipping swapchain recreation because the presentation window is closing");
            return;
        }

        std::array<VkFence, k_max_frames_in_flight> in_flight_fences{ };
        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
            in_flight_fences[i] = _frames[i].in_flight;

        const uint64_t fences_start_ms{ current_time_ms() };
        VK_CHECK_FATAL(vkWaitForFences(_device->vk_device(),
                                       static_cast<uint32_t>(in_flight_fences.size()),
                                       in_flight_fences.data(),
                                       VK_TRUE,
                                       UINT64_MAX));
        LOG_GRAPHICS_INFO("vkWaitForFences(recreate_swapchain_dependent_resources) completed in {} ms",
                          elapsed_ms(fences_start_ms));

        _framebuffers = framebuffer_array_t{ _device->vk_device() };

        if (_pending_resize_width > 0 && _pending_resize_height > 0)
            _swapchain->resize(_pending_resize_width, _pending_resize_height);
        else
            _swapchain->recreate();

        recreate_render_finished_semaphores();

        _framebuffers = _swapchain->create_framebuffers(_render_pass->vk_render_pass());

        LOG_GRAPHICS_INFO("Swapchain & framebuffers recreated after resize in {} ms (new size {}x{})",
                          elapsed_ms(recreate_start_ms),
                          _swapchain ? _swapchain->get_width() : 0,
                          _swapchain ? _swapchain->get_height() : 0);
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

    void vulkan_rhi_context_t::create_vk_buffer(const VkDeviceSize size, const VkBufferUsageFlags usage,
                                                const VkMemoryPropertyFlags properties, VkBuffer& out_buffer,
                                                VkDeviceMemory& out_memory) const
    {
        VkBufferCreateInfo buffer_info{ };
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_FATAL(vkCreateBuffer(_device->vk_device(), &buffer_info, nullptr, &out_buffer));

        VkMemoryRequirements mem_requirements{ };
        vkGetBufferMemoryRequirements(_device->vk_device(), out_buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

        VK_CHECK_FATAL(vkAllocateMemory(_device->vk_device(), &alloc_info, nullptr, &out_memory));
        VK_CHECK_FATAL(vkBindBufferMemory(_device->vk_device(), out_buffer, out_memory, 0));
    }

    void vulkan_rhi_context_t::copy_buffer(VkBuffer src, VkBuffer dst, const VkDeviceSize size) const
    {
        VkCommandBuffer cmd{ begin_single_time_commands() };

        VkBufferCopy copy_region{ };
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;

        vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);

        end_single_time_commands(cmd);
    }

    void vulkan_rhi_context_t::create_descriptor_pool()
    {
        std::array<VkDescriptorPoolSize, 3> pool_sizes{ };

        // Set 0: one camera UBO per frame-in-flight stage slot
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = k_max_frames_in_flight * k_max_textured_quad_stage_records_per_frame;

        // Set 1, binding 0: sampled image per textured-quad descriptor set
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_sizes[1].descriptorCount = k_max_textured_quad_descriptor_sets;

        // Set 1, binding 1: sampler per textured-quad descriptor set
        pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_sizes[2].descriptorCount = k_max_textured_quad_descriptor_sets;

        VkDescriptorPoolCreateInfo pool_info{ };
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets =
            (k_max_frames_in_flight * k_max_textured_quad_stage_records_per_frame) + k_max_textured_quad_descriptor_sets;

        VK_CHECK_FATAL(vkCreateDescriptorPool(_device->vk_device(), &pool_info, nullptr, &_descriptor_pool));

        LOG_GRAPHICS_INFO("Textured quad descriptor pool created successfully");
    }

    void vulkan_rhi_context_t::destroy_descriptor_pool() noexcept
    {
        for (auto& frame_descriptor_sets: _textured_quad.camera_descriptor_sets)
            frame_descriptor_sets.fill(VK_NULL_HANDLE);

        for (auto& frame_sets: _textured_quad.descriptor_sets)
            frame_sets.clear();

        if (_descriptor_pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(_device->vk_device(), _descriptor_pool, nullptr);
            _descriptor_pool = VK_NULL_HANDLE;
        }
    }

    void vulkan_rhi_context_t::release_textured_quad_resources() noexcept
    {
        _textured_quad.vertex_buffer = nullptr;
        _textured_quad.index_buffer = nullptr;
        _textured_quad.batches.clear();

        for (auto& frame_sets: _textured_quad.descriptor_sets)
            frame_sets.clear();

        for (auto& frame_descriptor_sets: _textured_quad.camera_descriptor_sets)
            frame_descriptor_sets.fill(VK_NULL_HANDLE);

        for (auto& frame_buffers: _textured_quad.camera_uniform_buffers)
            for (auto& buffer: frame_buffers)
                buffer.reset();
    }

    void vulkan_rhi_context_t::allocate_textured_quad_descriptor_sets(const uint32_t frame_index, const uint32_t count)
    {
        if (count == 0) return;

        std::vector<VkDescriptorSet>& frame_sets{ _textured_quad.descriptor_sets[frame_index] };
        const size_t old_size{ frame_sets.size() };
        frame_sets.resize(old_size + count, VK_NULL_HANDLE);

        const std::vector<VkDescriptorSetLayout> layouts(
            count, _textured_quad_pipeline->vk_texture_descriptor_set_layout());

        VkDescriptorSetAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = _descriptor_pool;
        alloc_info.descriptorSetCount = count;
        alloc_info.pSetLayouts = layouts.data();

        const VkResult result{
            vkAllocateDescriptorSets(_device->vk_device(), &alloc_info, frame_sets.data() + old_size)
        };

        if (result != VK_SUCCESS)
        {
            LOG_GRAPHICS_FATAL("Failed to allocate {} textured quad descriptor sets for frame {} (VkResult={})", count,
                               frame_index, static_cast<int>(result));

            frame_sets.resize(old_size);
            return;
        }

        LOG_GRAPHICS_INFO("Allocated {} textured quad descriptor set(s) for frame {} (total now {})", count,
                          frame_index, frame_sets.size());
    }

    void vulkan_rhi_context_t::ensure_textured_quad_descriptor_sets_for_frame(const uint32_t frame_index,
                                                                              const uint32_t batch_count)
    {
        const std::vector<VkDescriptorSet>& frame_sets{ _textured_quad.descriptor_sets[frame_index] };

        if (frame_sets.size() >= batch_count)
            return;

        const uint32_t missing{ batch_count - static_cast<uint32_t>(frame_sets.size()) };
        allocate_textured_quad_descriptor_sets(frame_index, missing);
    }

    void vulkan_rhi_context_t::write_textured_quad_descriptor_set(VkDescriptorSet descriptor_set,
                                                                  const rhi_texture_t& texture,
                                                                  const rhi_sampler_t& sampler) const
    {
        const vulkan_texture_t* vk_texture{ dynamic_cast<const vulkan_texture_t*>(&texture) };
        if (vk_texture == nullptr)
        {
            LOG_GRAPHICS_FATAL("write_textured_quad_descriptor_set received non-Vulkan texture");
            return;
        }

        const vulkan_sampler_t* vk_sampler_wrapper{ dynamic_cast<const vulkan_sampler_t*>(&sampler) };
        if (vk_sampler_wrapper == nullptr)
        {
            LOG_GRAPHICS_FATAL("write_textured_quad_descriptor_set received non-Vulkan sampler");
            return;
        }

        VkDescriptorImageInfo texture_info{ };
        texture_info.sampler = VK_NULL_HANDLE;
        texture_info.imageView = vk_texture->view();
        texture_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo sampler_info{ };
        sampler_info.sampler = vk_sampler_wrapper->vk_sampler();
        sampler_info.imageView = VK_NULL_HANDLE;
        sampler_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        std::array<VkWriteDescriptorSet, 2> writes{ };

        // Set 1, binding 0 -> Texture2D
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptor_set;
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[0].pImageInfo = &texture_info;

        // Set 1, binding 1 -> SamplerState
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptor_set;
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[1].pImageInfo = &sampler_info;

        vkUpdateDescriptorSets(_device->vk_device(),
                               static_cast<uint32_t>(writes.size()),
                               writes.data(),
                               0,
                               nullptr);
    }

    void vulkan_rhi_context_t::allocate_textured_quad_camera_descriptor_sets()
    {
        if (_textured_quad_pipeline == nullptr)
        {
            LOG_GRAPHICS_FATAL("Cannot allocate textured quad camera descriptor sets: pipeline not initialized");
            return;
        }

        if (_descriptor_pool == VK_NULL_HANDLE)
        {
            LOG_GRAPHICS_FATAL("Cannot allocate textured quad camera descriptor sets: descriptor pool not initialized");
            return;
        }

        constexpr uint32_t k_camera_descriptor_set_count{
            k_max_frames_in_flight * k_max_textured_quad_stage_records_per_frame
        };

        std::array<VkDescriptorSetLayout, k_camera_descriptor_set_count> layouts{ };
        layouts.fill(_textured_quad_pipeline->vk_camera_descriptor_set_layout());

        std::array<VkDescriptorSet, k_camera_descriptor_set_count> descriptor_sets{ };

        VkDescriptorSetAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = _descriptor_pool;
        alloc_info.descriptorSetCount = k_camera_descriptor_set_count;
        alloc_info.pSetLayouts = layouts.data();

        VK_CHECK_FATAL(vkAllocateDescriptorSets(
            _device->vk_device(),
            &alloc_info,
            descriptor_sets.data()));

        uint32_t descriptor_index{ 0 };
        for (uint32_t frame_index{ 0 }; frame_index < k_max_frames_in_flight; ++frame_index)
            for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_records_per_frame; ++stage_slot)
                _textured_quad.camera_descriptor_sets[frame_index][stage_slot] = descriptor_sets[descriptor_index++];
    }

    void vulkan_rhi_context_t::write_textured_quad_camera_descriptor_set(const uint32_t frame_index,
                                                                         const uint32_t stage_slot) const
    {
        const auto* camera_buffer = dynamic_cast<const vulkan_buffer_t*>(
            _textured_quad.camera_uniform_buffers[frame_index][stage_slot].get());

        if (camera_buffer == nullptr)
        {
            LOG_GRAPHICS_FATAL("Textured quad camera uniform buffer was not a Vulkan buffer");
            return;
        }

        VkDescriptorBufferInfo buffer_info{ };
        buffer_info.buffer = camera_buffer->vk_buffer();
        buffer_info.offset = 0;
        buffer_info.range = sizeof(renderer::textured_quad_camera_uniform_t);

        VkWriteDescriptorSet write{ };
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _textured_quad.camera_descriptor_sets[frame_index][stage_slot];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(_device->vk_device(), 1, &write, 0, nullptr);
    }

} // namespace carrot::rhi::vulkan
