//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DebugOverlay.h"

#include "IO/VirtualFileSystem.h"
#include "Renderer/Draw/TexturedQuadTypes.h"
#include "Renderer/Renderer.h"
#include "RHI/RHI.h"
#include "RHI/Texture.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace carrot::debug {
    namespace {
        constexpr int k_first_char{ 32 };
        constexpr int k_char_count{ 96 };
        constexpr int k_atlas_width{ 512 };
        constexpr int k_atlas_height{ 512 };
        constexpr float k_font_pixel_height{ 24.0f };

        renderer::renderer_t* g_renderer{ nullptr };
        std::unique_ptr<rhi::rhi_texture_t> g_font_texture{ nullptr };
        stbtt_bakedchar g_baked_chars[k_char_count]{ };
        bool g_initialized{ false };

        struct overlay_transform_t
        {
            chlm::float2 origin_world{ 0.f, 0.f };
            chlm::float2 units_per_pixel{ 1.f, 1.f };
        };

        [[nodiscard]] std::vector<uint8_t> load_file_bytes(const std::filesystem::path& path) noexcept
        {
            std::ifstream file{ path, std::ios::binary | std::ios::ate };
            if (!file)
                return {};

            const std::streamsize size{ file.tellg() };
            if (size <= 0)
                return {};

            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);

            if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
                return {};

            return bytes;
        }

        [[nodiscard]] std::vector<uint8_t> bake_font_rgba(const std::vector<uint8_t>& ttf_data) noexcept
        {
            if (ttf_data.empty())
                return {};

            std::vector<uint8_t> grayscale(static_cast<size_t>(k_atlas_width * k_atlas_height), 0u);
            const int bake_result{ stbtt_BakeFontBitmap(ttf_data.data(),
                                                        0,
                                                        k_font_pixel_height,
                                                        grayscale.data(),
                                                        k_atlas_width,
                                                        k_atlas_height,
                                                        k_first_char,
                                                        k_char_count,
                                                        g_baked_chars) };

            if (bake_result <= 0)
                return {};

            std::vector<uint8_t> rgba(static_cast<size_t>(k_atlas_width * k_atlas_height * 4), 0u);
            for (size_t i = 0; i < grayscale.size(); ++i)
            {
                const uint8_t coverage{ grayscale[i] };
                const size_t base{ i * 4u };
                rgba[base + 0u] = 0xFFu;
                rgba[base + 1u] = 0xFFu;
                rgba[base + 2u] = 0xFFu;
                rgba[base + 3u] = coverage;
            }

            return rgba;
        }

        [[nodiscard]] overlay_transform_t compute_overlay_transform() noexcept
        {
            overlay_transform_t transform{ };

            if (g_renderer == nullptr)
                return transform;

            const renderer::camera_2d_t& camera{ g_renderer->get_camera_2d() };
            const renderer::resolved_camera_2d_t resolved_camera{ g_renderer->resolve_camera_2d() };

            const float viewport_width{ static_cast<float>(std::max(1u, resolved_camera.viewport_rect_px.size.x)) };
            const float viewport_height{ static_cast<float>(std::max(1u, resolved_camera.viewport_rect_px.size.y)) };

            transform.origin_world = camera.position;
            transform.units_per_pixel = {
                resolved_camera.visible_world_size.x / viewport_width,
                resolved_camera.visible_world_size.y / viewport_height
            };

            return transform;
        }
    } // anonymous namespace

    void init(renderer::renderer_t* renderer, const io::virtual_file_system_t& vfs) noexcept
    {
        if (g_initialized)
            return;

        if (renderer == nullptr || renderer->get_rhi() == nullptr)
        {
            LOG_GRAPHICS_WARN("Debug overlay init skipped because renderer/RHI was not ready.");
            return;
        }

        const std::optional<std::filesystem::path> font_path{ vfs.resolve_native_path("engine://fonts/Roboto-Regular.ttf") };
        if (!font_path)
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: could not resolve built-in font.");
            return;
        }

        const std::vector<uint8_t> ttf_data{ load_file_bytes(*font_path) };
        if (ttf_data.empty())
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: could not read font bytes from '{}'.",
                              font_path->string());
            return;
        }

        const std::vector<uint8_t> rgba_pixels{ bake_font_rgba(ttf_data) };
        if (rgba_pixels.empty())
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: stb_truetype could not bake the font atlas.");
            return;
        }

        rhi::texture_create_info_t texture_info{ };
        texture_info.width = static_cast<uint32_t>(k_atlas_width);
        texture_info.height = static_cast<uint32_t>(k_atlas_height);
        texture_info.format = rhi::texture_format_t::rgba8_unorm;
        texture_info.initial_data = rgba_pixels.data();
        texture_info.initial_data_size = rgba_pixels.size();
        texture_info.initial_data_stride_bytes = static_cast<uint32_t>(k_atlas_width * 4);

        g_font_texture = renderer->get_rhi()->create_texture_2d(texture_info);
        if (!g_font_texture)
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: could not create the baked font texture.");
            return;
        }

        g_renderer = renderer;
        g_initialized = true;

        LOG_GRAPHICS_INFO("Debug overlay initialized with baked Roboto font atlas.");
    }

    void shutdown() noexcept
    {
        g_font_texture.reset();
        g_renderer = nullptr;
        g_initialized = false;
        std::fill(std::begin(g_baked_chars), std::end(g_baked_chars), stbtt_bakedchar{ });
    }

    bool is_initialized() noexcept
    {
        return g_initialized && g_renderer != nullptr && g_font_texture != nullptr;
    }

    void text(const float x, const float y, const char* fmt, ...) noexcept
    {
        if (!is_initialized() || fmt == nullptr)
            return;

        std::array<char, 1024> buffer{ };

        va_list args;
        va_start(args, fmt);
        const int written{ std::vsnprintf(buffer.data(), buffer.size(), fmt, args) };
        va_end(args);

        if (written <= 0)
            return;

        float pen_x{ x };
        float pen_y{ y };
        const float line_height{ k_font_pixel_height + 6.0f };
        const overlay_transform_t transform{ compute_overlay_transform() };

        const size_t text_length{ std::min(static_cast<size_t>(written), buffer.size() - 1u) };
        for (size_t i = 0; i < text_length; ++i)
        {
            char c{ buffer[i] };

            if (c == '\0')
                break;

            if (c == '\n')
            {
                pen_x = x;
                pen_y += line_height;
                continue;
            }

            const unsigned char codepoint{ static_cast<unsigned char>(c) };
            if (codepoint < static_cast<unsigned char>(k_first_char) ||
                codepoint >= static_cast<unsigned char>(k_first_char + k_char_count))
            {
                c = '?';
            }

            stbtt_aligned_quad quad{ };
            stbtt_GetBakedQuad(g_baked_chars,
                               k_atlas_width,
                               k_atlas_height,
                               c - k_first_char,
                               &pen_x,
                               &pen_y,
                               &quad,
                               0);

            renderer::textured_quad_draw_info_t glyph{ };
            glyph.texture = g_font_texture.get();
            glyph.x = transform.origin_world.x + (quad.x0 * transform.units_per_pixel.x);
            glyph.y = transform.origin_world.y + (quad.y0 * transform.units_per_pixel.y);
            glyph.width = (quad.x1 - quad.x0) * transform.units_per_pixel.x;
            glyph.height = (quad.y1 - quad.y0) * transform.units_per_pixel.y;
            glyph.u0 = quad.s0;
            glyph.v0 = quad.t0;
            glyph.u1 = quad.s1;
            glyph.v1 = quad.t1;
            glyph.layer = renderer::render_layer_t::debug;
            glyph.color = 0xFFFFFFFFu;
            glyph.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;

            if (glyph.width > 0.f && glyph.height > 0.f)
                g_renderer->draw_textured_quad(glyph);
        }
    }
} // namespace carrot::debug
