//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "DebugOverlay.h"

#include "RHI/Backends/Vulkan/VulkanRenderer.h"
#include "RHI/Backends/Vulkan/VulkanContext.h"
#include "Utils/ShaderUtils.h"
#include "Common/CommonHeaders.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <cstdarg>
#include <vector>
#include <fstream>

namespace carrot::debug {
    namespace {
        constexpr int k_atlas_width = 1024;
        constexpr int k_atlas_height = 1024;
        constexpr int k_first_char = 32;
        constexpr int k_char_count = 96;
        constexpr float k_font_pixel_height = 32.0f;

        // stbtt_fontinfo g_font;
        unsigned char* g_ttf_buffer{ nullptr };
        unsigned char* g_atlas_pixels{ nullptr };
        bool g_initialized{ false };

        struct glyph_t
        {
            float x0, y0, x1, y1; // atlas coords (pixels)
            float xoff, yoff; // bearing
            float advance;
        };

        glyph_t g_glyphs[k_char_count];

        VkImage g_font_image{ VK_NULL_HANDLE };
        VkImageView g_font_view{ VK_NULL_HANDLE };
        VkDeviceMemory g_font_memory{ VK_NULL_HANDLE };
        VkSampler g_font_sampler{ VK_NULL_HANDLE };

        VkDescriptorSetLayout g_desc_layout{ VK_NULL_HANDLE };
        VkDescriptorPool g_desc_pool{ VK_NULL_HANDLE };
        VkDescriptorSet g_desc_set{ VK_NULL_HANDLE };

        VkPipelineLayout g_pipeline_layout{ VK_NULL_HANDLE };
        VkPipeline g_pipeline{ VK_NULL_HANDLE };

        struct vertex_t
        {
            float x, y, u, v;
        };

        std::vector<vertex_t> g_vertices;
        std::vector<uint16_t> g_indices;

        VkBuffer g_vb{ VK_NULL_HANDLE };
        VkDeviceMemory g_vb_mem{ VK_NULL_HANDLE };
        VkBuffer g_ib{ VK_NULL_HANDLE };
        VkDeviceMemory g_ib_mem{ VK_NULL_HANDLE };

        void create_font_texture()
        {
            rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };

            // ── Create staging buffer
            VkDeviceSize atlas_size{ k_atlas_width * k_atlas_height };

            VkBuffer staging_buf{ };
            VkDeviceMemory staging_mem{ };

            VkBufferCreateInfo buf_info{ };
            buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buf_info.size = atlas_size;
            buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            vkCreateBuffer(ctx->device, &buf_info, nullptr, &staging_buf);

            VkMemoryRequirements mem_req{ };
            vkGetBufferMemoryRequirements(ctx->device, staging_buf, &mem_req);

            VkMemoryAllocateInfo alloc_info{ };
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = mem_req.size;
            alloc_info.memoryTypeIndex = ctx->find_memory_type(mem_req.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            vkAllocateMemory(ctx->device, &alloc_info, nullptr, &staging_mem);
            vkBindBufferMemory(ctx->device, staging_buf, staging_mem, 0);

            void* mapped;
            vkMapMemory(ctx->device, staging_mem, 0, atlas_size, 0, &mapped);
            memcpy(mapped, g_atlas_pixels, atlas_size);
            vkUnmapMemory(ctx->device, staging_mem);

            // ── Create device-local image
            VkImageCreateInfo img_info{ };
            img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            img_info.imageType = VK_IMAGE_TYPE_2D;
            img_info.format = VK_FORMAT_R8_UNORM;
            img_info.extent = { k_atlas_width, k_atlas_height, 1 };
            img_info.mipLevels = 1;
            img_info.arrayLayers = 1;
            img_info.samples = VK_SAMPLE_COUNT_1_BIT;
            img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            img_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkCreateImage(ctx->device, &img_info, nullptr, &g_font_image);

            vkGetImageMemoryRequirements(ctx->device, g_font_image, &mem_req);
            alloc_info.allocationSize = mem_req.size;
            alloc_info.memoryTypeIndex = ctx->find_memory_type(mem_req.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(ctx->device, &alloc_info, nullptr, &g_font_memory);
            vkBindImageMemory(ctx->device, g_font_image, g_font_memory, 0);

            // ── Transition + copy
            VkCommandBuffer cmd{ ctx->begin_one_time_commands() };

            VkImageMemoryBarrier barrier{ };
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = g_font_image;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy region{ };
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = { k_atlas_width, k_atlas_height, 1 };
            vkCmdCopyBufferToImage(cmd, staging_buf, g_font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            ctx->end_one_time_commands(cmd);

            // cleanup staging
            vkDestroyBuffer(ctx->device, staging_buf, nullptr);
            vkFreeMemory(ctx->device, staging_mem, nullptr);

            // view + sampler
            VkImageViewCreateInfo view_info{ };
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = g_font_image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = VK_FORMAT_R8_UNORM;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            vkCreateImageView(ctx->device, &view_info, nullptr, &g_font_view);

            VkSamplerCreateInfo sampler_info{ };
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.minLod = 0.0f;
            sampler_info.maxLod = 1.0f;
            vkCreateSampler(ctx->device, &sampler_info, nullptr, &g_font_sampler);
        }

        void create_descriptor_objects()
        {
            const rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };

            VkDescriptorSetLayoutBinding binding{ };
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info{ };
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = 1;
            layout_info.pBindings = &binding;
            vkCreateDescriptorSetLayout(ctx->device, &layout_info, nullptr, &g_desc_layout);

            constexpr VkDescriptorPoolSize pool_size{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
            VkDescriptorPoolCreateInfo pool_info{ };
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.maxSets = 1;
            pool_info.poolSizeCount = 1;
            pool_info.pPoolSizes = &pool_size;
            vkCreateDescriptorPool(ctx->device, &pool_info, nullptr, &g_desc_pool);

            VkDescriptorSetAllocateInfo alloc_info{ };
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = g_desc_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &g_desc_layout;
            vkAllocateDescriptorSets(ctx->device, &alloc_info, &g_desc_set);

            VkDescriptorImageInfo img_info{ };
            img_info.sampler = g_font_sampler;
            img_info.imageView = g_font_view;
            img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write{ };
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = g_desc_set;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &img_info;
            vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nullptr);
        }

        void create_pipeline()
        {
            rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };

            std::vector<uint32_t> vert_spv{ load_spv("shaders/debug_overlay.vert.spv") };
            std::vector<uint32_t> frag_spv{ load_spv("shaders/debug_overlay.frag.spv") };

            VkShaderModule vert_mod{ VK_NULL_HANDLE };
            VkShaderModule frag_mod{ VK_NULL_HANDLE };

            VkShaderModuleCreateInfo mod_info{ };
            mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            mod_info.codeSize = vert_spv.size() * sizeof(uint32_t);
            mod_info.pCode = vert_spv.data();
            vkCreateShaderModule(ctx->device, &mod_info, nullptr, &vert_mod);

            mod_info.codeSize = frag_spv.size() * sizeof(uint32_t);
            mod_info.pCode = frag_spv.data();
            vkCreateShaderModule(ctx->device, &mod_info, nullptr, &frag_mod);

            VkPipelineShaderStageCreateInfo stages[2]{
                {
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
                    vert_mod, "main", nullptr
                },
                {
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
                    frag_mod, "main", nullptr
                }
            };

            VkVertexInputBindingDescription binding{ 0, sizeof(vertex_t), VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription attrs[2]{
                { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_t, x) },
                { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_t, u) }
            };
            VkPipelineVertexInputStateCreateInfo vertex_input{ };
            vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input.vertexBindingDescriptionCount = 1;
            vertex_input.pVertexBindingDescriptions = &binding;
            vertex_input.vertexAttributeDescriptionCount = 2;
            vertex_input.pVertexAttributeDescriptions = attrs;

            VkPipelineInputAssemblyStateCreateInfo ia{ };
            ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo vp{ };
            vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            vp.viewportCount = vp.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rs{ };
            rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rs.lineWidth = 1.0f;
            rs.cullMode = VK_CULL_MODE_NONE;
            rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

            VkPipelineMultisampleStateCreateInfo ms{ };
            ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState blend{ };
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorWriteMask = 0xF;

            VkPipelineColorBlendStateCreateInfo cb{ };
            cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            cb.attachmentCount = 1;
            cb.pAttachments = &blend;

            VkDynamicState dyn[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamic{ };
            dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dyn;

            // PUSH CONSTANT + DESCRIPTOR SET
            VkPushConstantRange push_range{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 2 };

            VkPipelineLayoutCreateInfo layout_info{ };
            layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_info.setLayoutCount = 1;
            layout_info.pSetLayouts = &g_desc_layout;
            layout_info.pushConstantRangeCount = 1;
            layout_info.pPushConstantRanges = &push_range;
            vkCreatePipelineLayout(ctx->device, &layout_info, nullptr, &g_pipeline_layout);

            VkGraphicsPipelineCreateInfo pipe{ };
            pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipe.stageCount = 2;
            pipe.pStages = stages;
            pipe.pVertexInputState = &vertex_input;
            pipe.pInputAssemblyState = &ia;
            pipe.pViewportState = &vp;
            pipe.pRasterizationState = &rs;
            pipe.pMultisampleState = &ms;
            pipe.pColorBlendState = &cb;
            pipe.pDynamicState = &dynamic;
            pipe.layout = g_pipeline_layout;
            pipe.renderPass = ctx->render_pass;
            pipe.subpass = 0;

            vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe, nullptr, &g_pipeline);

            vkDestroyShaderModule(ctx->device, vert_mod, nullptr);
            vkDestroyShaderModule(ctx->device, frag_mod, nullptr);
        }

        void rasterize_text(float x, float y, const char* text)
        {
            const float start_x{ x };

            for (; *text; ++text)
            {
                if (*text == '\n')
                {
                    x = start_x;
                    y += k_font_pixel_height * 1.4f;
                    continue;
                }
                if (static_cast<unsigned char>(*text) < k_first_char ||
                    static_cast<unsigned char>(*text) >= k_first_char + k_char_count)
                    continue;

                const int idx{ *text - k_first_char };
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(reinterpret_cast<stbtt_bakedchar *>(g_glyphs), k_atlas_width, k_atlas_height,
                                   idx, &x, &y, &q, 1);

                const uint16_t base{ static_cast<uint16_t>(g_vertices.size()) };

                g_vertices.push_back({ q.x0, q.y0, q.s0, q.t0 });
                g_vertices.push_back({ q.x1, q.y0, q.s1, q.t0 });
                g_vertices.push_back({ q.x1, q.y1, q.s1, q.t1 });
                g_vertices.push_back({ q.x0, q.y1, q.s0, q.t1 });

                g_indices.insert(g_indices.end(), {
                                     static_cast<uint16_t>(base + 0), static_cast<uint16_t>(base + 1),
                                     static_cast<uint16_t>(base + 2),
                                     static_cast<uint16_t>(base + 0), static_cast<uint16_t>(base + 2),
                                     static_cast<uint16_t>(base + 3)
                                 });
            }
        }
    } // anonymous namespace

    void init() noexcept
    {
        FILE* f = fopen("assets/Fonts/Roboto-Regular.ttf", "rb");
        if (!f)
        {
            LOG_GRAPHICS_ERROR("Failed to open Roboto-Regular.ttf");
            return;
        }

        fseek(f, 0, SEEK_END);
        const size_t sz{ static_cast<size_t>(ftell(f)) };
        fseek(f, 0, SEEK_SET);
        g_ttf_buffer = new unsigned char[sz];
        fread(g_ttf_buffer, 1, sz, f);
        fclose(f);

        g_atlas_pixels = new unsigned char[k_atlas_width * k_atlas_height];
        stbtt_BakeFontBitmap(g_ttf_buffer, 0, k_font_pixel_height,
                             g_atlas_pixels, k_atlas_width, k_atlas_height,
                             k_first_char, k_char_count, reinterpret_cast<stbtt_bakedchar *>(g_glyphs));

        const rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };

        create_font_texture();
        create_descriptor_objects();
        create_pipeline();

        // After create_pipeline() — CREATE BUFFERS AFTER EVERYTHING ELSE EXISTS
        constexpr VkDeviceSize vb_size{ 16 * 1024 * 1024 }; // 16 MiB
        constexpr VkDeviceSize ib_size{ 8 * 1024 * 1024 }; // 8 MiB

        // Vertex buffer
        VkBufferCreateInfo buf_info{ };
        buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_info.size = vb_size;
        buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(ctx->device, &buf_info, nullptr, &g_vb);

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(ctx->device, g_vb, &mem_req);

        VkMemoryAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = ctx->find_memory_type(mem_req.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(ctx->device, &alloc_info, nullptr, &g_vb_mem);
        vkBindBufferMemory(ctx->device, g_vb, g_vb_mem, 0);

        // Index buffer
        buf_info.size = ib_size;
        buf_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        vkCreateBuffer(ctx->device, &buf_info, nullptr, &g_ib);

        vkGetBufferMemoryRequirements(ctx->device, g_ib, &mem_req);
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = ctx->find_memory_type(mem_req.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(ctx->device, &alloc_info, nullptr, &g_ib_mem);
        vkBindBufferMemory(ctx->device, g_ib, g_ib_mem, 0);

        g_initialized = true; // ← SET THIS AT THE END
        LOG_GRAPHICS_INFO("DEBUG OVERLAY FULLY INITIALIZED — READY TO RENDER");
    }

    void shutdown() noexcept
    {
        const rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };
        vkDeviceWaitIdle(ctx->device);

        vkDestroyPipeline(ctx->device, g_pipeline, nullptr);
        vkDestroyPipelineLayout(ctx->device, g_pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(ctx->device, g_desc_layout, nullptr);
        vkDestroyDescriptorPool(ctx->device, g_desc_pool, nullptr);
        vkDestroySampler(ctx->device, g_font_sampler, nullptr);
        vkDestroyImageView(ctx->device, g_font_view, nullptr);
        vkDestroyImage(ctx->device, g_font_image, nullptr);
        vkFreeMemory(ctx->device, g_font_memory, nullptr);
        vkDestroyBuffer(ctx->device, g_vb, nullptr);
        vkFreeMemory(ctx->device, g_vb_mem, nullptr);
        vkDestroyBuffer(ctx->device, g_ib, nullptr);
        vkFreeMemory(ctx->device, g_ib_mem, nullptr);

        delete[] g_ttf_buffer;
        delete[] g_atlas_pixels;
    }

    void render() noexcept
    {
        if (g_vertices.empty()) return;

        const rhi::vulkan_context_t* ctx{ rhi::vulkan_context_t::get() };
        VkCommandBuffer cmd{ vulkan_renderer_t::get_current_command_buffer() };

        constexpr float resolution[2]{ 1280.0f, 720.0f };
        vkCmdPushConstants(cmd, g_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(resolution), resolution);

        // Upload real text geometry
        void* data;
        vkMapMemory(ctx->device, g_vb_mem, 0, g_vertices.size() * sizeof(vertex_t), 0, &data);
        memcpy(data, g_vertices.data(), g_vertices.size() * sizeof(vertex_t));
        vkUnmapMemory(ctx->device, g_vb_mem);

        vkMapMemory(ctx->device, g_ib_mem, 0, g_indices.size() * sizeof(uint16_t), 0, &data);
        memcpy(data, g_indices.data(), g_indices.size() * sizeof(uint16_t));
        vkUnmapMemory(ctx->device, g_ib_mem);

        constexpr VkDeviceSize offset{ 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &g_vb, &offset);
        vkCmdBindIndexBuffer(cmd, g_ib, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline_layout, 0, 1, &g_desc_set, 0, nullptr);

        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(g_indices.size()), 1, 0, 0, 0);

        g_vertices.clear();
        g_indices.clear();
    }

    bool is_initialized() noexcept
    {
        return g_initialized;
    }

    void text(float x, float y, const char* fmt, ...) noexcept
    {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        rasterize_text(x, y, buffer);
    }
} // namespace carrot::debug
