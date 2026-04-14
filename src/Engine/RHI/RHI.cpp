//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "RHI/RHI.h"

#include "Backends/Null/NullRHIContext.h"
#include "Backends/Vulkan/VulkanRHIContext.h"

#if defined(CARROT_PLATFORM_COCOA)
#include "Backends/Metal/MetalRHIContext.h"
#endif
#if defined(CARROT_PLATFORM_WIN32)
#include "Backends/DirectX12/DirectX12RHIContext.h"
#endif

namespace carrot::rhi {
    namespace {
        bool is_api_supported(graphics_api api)
        {
            if (api == graphics_api::null_backend)
                return true;

#if defined(CARROT_PLATFORM_WAYLAND) || defined(CARROT_PLATFORM_X11)
            return api == graphics_api::vulkan;
#elif defined(CARROT_PLATFORM_WIN32)
            return api == graphics_api::direct_x12 || api == graphics_api::vulkan;
#elif defined(CARROT_PLATFORM_COCOA)
            return api == graphics_api::metal || api == graphics_api::vulkan;
#endif
        }

        graphics_api choose_default_api()
        {
#if defined(CARROT_PLATFORM_WAYLAND) || defined(CARROT_PLATFORM_X11)
            return graphics_api::vulkan;
#elif defined(CARROT_PLATFORM_WIN32)
            return graphics_api::direct_x12;
#elif defined(CARROT_PLATFORM_COCOA)
            return graphics_api::metal;
#endif
        }

        graphics_api resolve_graphics_api(const rhi_desc_t& desc)
        {
            if (desc.api == graphics_api::default_api)
                return choose_default_api();

            if (!is_api_supported(desc.api))
            {
                LOG_GRAPHICS_WARN(
                    "Configured graphics API {} is not supported. Falling back.",
                    graphics_api_to_string(desc.api)
                );
                return choose_default_api();
            }

            return desc.api;
        }

    } // anonymous namespace

    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc)
    {
        switch (resolve_graphics_api(desc))
        {
            case graphics_api::null_backend:
                return std::make_unique<null::null_rhi_context_t>(desc);

            case graphics_api::vulkan:
                return std::make_unique<vulkan::vulkan_rhi_context_t>(desc);

            case graphics_api::direct_x12:
#if defined(CARROT_PLATFORM_WIN32)
                return std::make_unique<dx12::dx12_rhi_context_t>(desc);
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
