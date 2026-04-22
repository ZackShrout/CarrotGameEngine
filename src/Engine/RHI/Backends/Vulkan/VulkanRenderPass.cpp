//
// Created by zshrout on 1/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanRenderPass.h"

namespace carrot::rhi::vulkan {
    vulkan_render_pass_t::vulkan_render_pass_t(const vulkan_device_t* device,
                                               const VkFormat color_format,
                                               const VkAttachmentLoadOp load_op,
                                               const VkImageLayout initial_layout,
                                               const VkImageLayout final_layout) : _device{ device }
    {
        VkAttachmentDescription color_attachment{ };
        color_attachment.format = color_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = load_op;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = initial_layout;
        color_attachment.finalLayout = final_layout;

        VkAttachmentReference color_attachment_ref{ };
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{ };
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency dependency{ };
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info{ };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        VK_CHECK_FATAL(vkCreateRenderPass(
            _device->vk_device(),
            &render_pass_info,
            nullptr,
            &_render_pass
        ));

        LOG_GRAPHICS_INFO("Render pass created successfully");
    }

    vulkan_render_pass_t::~vulkan_render_pass_t()
    {
        if (_render_pass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(_device->vk_device(), _render_pass, nullptr);
            _render_pass = VK_NULL_HANDLE;
        }
    }
} // namespace carrot::rhi::vulkan
