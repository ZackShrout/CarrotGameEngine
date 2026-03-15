//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "../../../Core/CoreDefines.h"

#include <vulkan/vulkan.h>
#include <chlm/CarrotHLM.h>

namespace carrot::rhi::vulkan {
    inline const char* result_to_string(const VkResult result)
    {
        switch (result)
        {
#define CASE(x) case x: return #x
            CASE(VK_SUCCESS);
            CASE(VK_NOT_READY);
            CASE(VK_TIMEOUT);
            CASE(VK_EVENT_SET);
            CASE(VK_EVENT_RESET);
            CASE(VK_INCOMPLETE);
            CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
            CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            CASE(VK_ERROR_INITIALIZATION_FAILED);
            CASE(VK_ERROR_DEVICE_LOST);
            CASE(VK_ERROR_MEMORY_MAP_FAILED);
            CASE(VK_ERROR_LAYER_NOT_PRESENT);
            CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
            CASE(VK_ERROR_FEATURE_NOT_PRESENT);
            CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
            CASE(VK_ERROR_TOO_MANY_OBJECTS);
            CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
            CASE(VK_ERROR_FRAGMENTED_POOL);
            CASE(VK_ERROR_SURFACE_LOST_KHR);
            CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            CASE(VK_SUBOPTIMAL_KHR);
            CASE(VK_ERROR_OUT_OF_DATE_KHR);
            CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
            CASE(VK_ERROR_VALIDATION_FAILED_EXT);
            CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            CASE(VK_THREAD_IDLE_KHR);
            CASE(VK_THREAD_DONE_KHR);
            CASE(VK_OPERATION_DEFERRED_KHR);
            CASE(VK_OPERATION_NOT_DEFERRED_KHR);
            CASE(VK_PIPELINE_COMPILE_REQUIRED_EXT);
                // ... add more as needed
#undef CASE
            default: return "UNKNOWN_VK_RESULT";
        }
    }
} // namespace carrot::rhi::vulkan

// Always available, even in release
#define VK_CHECK_FATAL(result) \
    VK_CHECK_IMPL((result), true, "Vulkan fatal error")

#define VK_CHECK(result) \
    VK_CHECK_IMPL((result), false, "Vulkan error")

// Internal implementation
#define VK_CHECK_IMPL(result, is_fatal, prefix_msg)                                                 \
    do {                                                                                            \
        VkResult vk_check_impl_result{ (result) };                                                  \
        if (vk_check_impl_result != VK_SUCCESS) {                                                   \
            const char* error_str{ carrot::rhi::vulkan::result_to_string(vk_check_impl_result) };   \
            carrot::core::logger_t::log(std::source_location::current(),                            \
                carrot::core::log_category::graphics,                                               \
                is_fatal ? carrot::core::log_severity::fatal : carrot::core::log_severity::error,   \
                "{} ({}): {}", prefix_msg, #result, error_str);                                     \
            if (is_fatal) {                                                                         \
                CE_ASSERT(0);                                                                         \
            }                                                                                       \
        }                                                                                           \
    } while (0)

#define VK_CHECK_ACQUIRE(result)                                        \
    do {                                                                \
        VkResult r = (result);                                          \
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {  \
            recreate_swapchain_dependent_resources();                   \
            return;                                                     \
        } else {                                                        \
            VK_CHECK_FATAL(r);                                          \
        }                                                               \
    } while(0)

#ifdef _DEBUG
#define VK_EXPECT(result) VK_CHECK_FATAL(result)
#else
#define VK_EXPECT(result) (result)
#endif
