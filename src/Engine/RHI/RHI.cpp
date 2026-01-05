//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "RHI/RHI.h"
#include "RHI/Backends/Vulkan/VulkanRHIContext.h"
#include "RHI/Backends/Vulkan/VulkanRenderer.h"

namespace carrot::rhi {

    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc)
    {
        if (desc.api != graphics_api::vulkan)
        {
            LOG_CORE_FATAL("Only Vulkan backend supported at this time!");
            return nullptr;
        }

        vulkan::vulkan_renderer_t* renderer = nullptr;

        if (desc.existing_renderer)
        {
            renderer = dynamic_cast<vulkan::vulkan_renderer_t*>(desc.existing_renderer);
            if (!renderer)
            {
                LOG_CORE_FATAL("Existing renderer is not Vulkan!");
                return nullptr;
            }
            LOG_CORE_INFO("RHI: Reusing existing Vulkan renderer instance");
        }
        else
        {
            // Future: real creation path
            LOG_CORE_WARN("RHI: Creating new renderer not supported yet in hybrid mode");
            return nullptr;
        }

        return std::make_unique<vulkan_rhi_context_t>(renderer);
    }

} // namespace carrot::rhi