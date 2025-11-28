#pragma once

#include <vulkan/vulkan.h>

namespace carrot::rhi {

struct vulkan_context_t
{
    VkInstance       instance{ VK_NULL_HANDLE };
    VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
    VkDevice         device{ VK_NULL_HANDLE };
    VkSurfaceKHR     surface{ VK_NULL_HANDLE };
    uint32_t         graphics_family{ ~0u };
    uint32_t         present_family{ ~0u };
    VkQueue          graphics_queue{ VK_NULL_HANDLE };
    VkQueue          present_queue{ VK_NULL_HANDLE };
    VkSwapchainKHR   swapchain{ VK_NULL_HANDLE };
    VkFormat         swapchain_format{};
    VkExtent2D       swapchain_extent{};
    uint32_t         image_count{ 0 };
    uint32_t         frame_count{0};
    VkImage*         swapchain_images{ nullptr };
    VkImageView*     swapchain_views{ nullptr };

    void init(VkInstance inst, VkSurfaceKHR surf);
    void create_swapchain(uint32_t width, uint32_t height);
    void cleanup();
};

} // namespace carrot::rhi