//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "RHI/RHI.h"

#include "Backends/Vulkan/VulkanRHIContext.h"
#include "Common/CommonHeaders.h"

namespace carrot::rhi {

    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc)
    {
        switch (desc.api)
        {
            case graphics_api::vulkan: return std::make_unique<vulkan::vulkan_rhi_context_t>(desc);

            case graphics_api::direct_x12:
            case graphics_api::metal:
                LOG_CORE_FATAL("Backend {} not implemented yet", static_cast<int>(desc.api));
                return nullptr;

            default:
                LOG_CORE_FATAL("Invalid backend enum value");
                return nullptr;
        }

        return nullptr;
    }

} // namespace carrot::rhi