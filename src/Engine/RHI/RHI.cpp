//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "RHI/RHI.h"

#include "Backends/Vulkan/VulkanRHIContext.h"
#include "Backends/Metal/MetalRHIContext.h"
#include "Common/CommonHeaders.h"

namespace carrot::rhi {

    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc)
    {
        switch (desc.api)
        {
            case graphics_api::vulkan:
#if defined(CARROT_PLATFORM_WAYLAND) || defined(CARROT_PLATFORM_X11) || defined(CARROT_PLATFORM_WIN32)
                return std::make_unique<vulkan::vulkan_rhi_context_t>(desc);
#else
                LOG_GRAPHICS_FATAL("Backend {} not supported on Apple", graphics_api_to_string(desc.api));
                return nullptr;
#endif

            case graphics_api::direct_x12:
#if defined(CARROT_PLATFORM_WIN32)
                LOG_GRAPHICS_FATAL("Backend {} not implemented yet", graphics_api_to_string(desc.api));
                return nullptr;
#else
                LOG_GRAPHICS_FATAL("Backend {} only supported on Windows", graphics_api_to_string(desc.api));
                return nullptr;
#endif

            case graphics_api::metal:
#if defined(CARROT_PLATFORM_COCOA)
                return std::make_unique<metal::metal_rhi_context_t>(desc);
#else
                LOG_GRAPHICS_FATAL("Backend {} only supported on Apple", graphics_api_to_string(desc.api));
                return nullptr;
#endif

            default:
                LOG_GRAPHICS_FATAL("Invalid backend enum value");
                return nullptr;
        }
    }

} // namespace carrot::rhi
