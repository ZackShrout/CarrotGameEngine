//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DebugOverlay.h"

#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "RHI/Texture.h"
#include "Renderer/Draw/TexturedQuadTypes.h"
#include "Renderer/Renderer.h"

#include <cstdarg>
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

        [[nodiscard]] bool has_renderer() noexcept
        {
            return g_renderer != nullptr;
        }

        [[nodiscard]] uint32_t with_alpha(const uint32_t color_abgr, const uint8_t alpha) noexcept
        {
            return (color_abgr & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha) << 24u);
        }

        void submit_world_solid_quad(const float x,
                                     const float y,
                                     const float width,
                                     const float height,
                                     const uint32_t color) noexcept
        {
            if (!has_renderer() || width <= 0.f || height <= 0.f)
                return;

            g_renderer->draw_solid_quad(renderer::solid_quad_draw_info_t{
                .x = x,
                .y = y,
                .width = width,
                .height = height,
                .layer = renderer::render_layer_t::debug,
                .order_in_layer = 0,
                .color = color,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });
        }

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
    } // anonymous namespace

    void init(renderer::renderer_t* renderer, const io::virtual_file_system_t& vfs) noexcept
    {
        if (g_initialized && g_renderer == renderer)
            return;

        if (renderer == nullptr || renderer->get_rhi() == nullptr)
        {
            LOG_GRAPHICS_WARN("Debug overlay init skipped because renderer/RHI was not ready.");
            return;
        }

        g_renderer = renderer;

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

    namespace {
        void text_v(const float x,
                    const float y,
                    const uint32_t color,
                    const char* fmt,
                    va_list args) noexcept
        {
            if (!is_initialized() || fmt == nullptr)
                return;

            std::array<char, 1024> buffer{ };
            va_list args_copy;
            va_copy(args_copy, args);
            const int written{ std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy) };
            va_end(args_copy);

            if (written <= 0)
                return;

            float pen_x{ x };
            float pen_y{ y };
            const float line_height{ k_font_pixel_height + 6.0f };
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
                glyph.x = quad.x0;
                glyph.y = quad.y0;
                glyph.width = quad.x1 - quad.x0;
                glyph.height = quad.y1 - quad.y0;
                glyph.u0 = quad.s0;
                glyph.v0 = quad.t0;
                glyph.u1 = quad.s1;
                glyph.v1 = quad.t1;
                glyph.layer = renderer::render_layer_t::debug;
                glyph.color = color;
                glyph.sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp;

                if (glyph.width > 0.f && glyph.height > 0.f)
                    g_renderer->draw_overlay_textured_quad(glyph);
            }
        }
    } // namespace

    void text(const float x, const float y, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, 0xFFFFFFFFu, fmt, args);
        va_end(args);
    }

    void text_colored(const float x, const float y, const uint32_t color, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, color, fmt, args);
        va_end(args);
    }

    void world_rect(const float x,
                    const float y,
                    const float width,
                    const float height,
                    const world_rect_style_t style) noexcept
    {
        if (!has_renderer() || width <= 0.f || height <= 0.f)
            return;

        if (style.filled)
            submit_world_solid_quad(x, y, width, height, with_alpha(style.color, 0x44u));

        const float thickness{ std::max(1.0e-4f, std::min(style.outline_thickness, std::min(width, height))) };
        const float vertical_span{ std::max(0.f, height - (2.f * thickness)) };

        submit_world_solid_quad(x, y, width, thickness, style.color);
        submit_world_solid_quad(x, y + height - thickness, width, thickness, style.color);
        submit_world_solid_quad(x, y + thickness, thickness, vertical_span, style.color);
        submit_world_solid_quad(x + width - thickness, y + thickness, thickness, vertical_span, style.color);
    }

    void world_aabb(const collision::collision_aabb_t& bounds, const world_rect_style_t style) noexcept
    {
        const chlm::float2 size{ bounds.size() };
        world_rect(bounds.min.x, bounds.min.y, size.x, size.y, style);
    }
} // namespace carrot::debug
