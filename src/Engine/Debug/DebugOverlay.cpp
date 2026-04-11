//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DebugOverlay.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetService.h"
#include "Assets/Font/TextLayout.h"
#include "Renderer/Draw/TexturedQuadTypes.h"
#include "Renderer/Renderer.h"

#include <cstdarg>

namespace carrot::debug {
    namespace {
        constexpr float k_font_pixel_height{ 24.0f };
        constexpr std::string_view k_debug_font_id{ "font.engine.roboto_regular" };

        renderer::renderer_t* g_renderer{ nullptr };
        const assets::loaded_font_asset_t* g_font_asset{ nullptr };
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

    } // anonymous namespace

    void init(renderer::renderer_t* renderer, [[maybe_unused]] const io::virtual_file_system_t& vfs) noexcept
    {
        if (g_initialized && g_renderer == renderer)
            return;

        if (renderer == nullptr || renderer->get_rhi() == nullptr)
        {
            LOG_GRAPHICS_WARN("Debug overlay init skipped because renderer/RHI was not ready.");
            return;
        }

        g_renderer = renderer;

        assets::asset_manager_t* asset_manager{ assets::asset_service_t::try_manager() };
        if (asset_manager == nullptr)
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: asset manager was not available.");
            return;
        }

        g_font_asset = asset_manager->fonts().get(k_debug_font_id);
        if (g_font_asset == nullptr || !g_font_asset->valid())
        {
            LOG_GRAPHICS_WARN("Debug overlay init failed: could not load debug font asset '{}'.",
                              k_debug_font_id);
            return;
        }

        g_initialized = true;

        LOG_GRAPHICS_INFO("Debug overlay initialized with cooked font asset '{}'.", k_debug_font_id);
    }

    void shutdown() noexcept
    {
        g_font_asset = nullptr;
        g_renderer = nullptr;
        g_initialized = false;
    }

    bool is_initialized() noexcept
    {
        return g_initialized && g_renderer != nullptr && g_font_asset != nullptr && g_font_asset->valid();
    }

    namespace {
        enum class debug_text_target_t : uint8_t
        {
            overlay = 0,
            log_console
        };

        void text_v(const float x,
                    const float y,
                    const uint32_t color,
                    const debug_text_target_t target,
                    const char* fmt,
                    va_list args) noexcept
        {
            if (!is_initialized() || fmt == nullptr)
                return;

            std::array<char, 1024> buffer{ };
            va_list args_copy;
            va_copy(args_copy, args);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
            const int written{ std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy) };
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
            va_end(args_copy);

            if (written <= 0)
                return;

            const size_t text_length{ std::min(static_cast<size_t>(written), buffer.size() - 1u) };
            const std::string_view text{ buffer.data(), text_length };
            const assets::text_layout_result_t layout{
                assets::layout_text(*g_font_asset,
                                    text,
                                    assets::text_layout_settings_t{
                                        .font_size = k_font_pixel_height,
                                        .wrap_width = 0.0f,
                                        .letter_spacing = 0.35f,
                                        .line_spacing = 6.0f,
                                    })
            };

            for (const assets::positioned_glyph_t& positioned : layout.glyphs)
            {
                if (positioned.glyph == nullptr || positioned.width <= 0.f || positioned.height <= 0.f)
                    continue;

                renderer::textured_quad_draw_info_t glyph{ };
                glyph.texture = g_font_asset->atlas_texture.get();
                glyph.x = x + positioned.x;
                glyph.y = y + positioned.y;
                glyph.width = positioned.width;
                glyph.height = positioned.height;
                glyph.u0 = positioned.u0;
                glyph.v0 = positioned.v0;
                glyph.u1 = positioned.u1;
                glyph.v1 = positioned.v1;
                glyph.layer = renderer::render_layer_t::debug;
                glyph.color = color;
                glyph.effect_param0 = g_font_asset->cooked.metrics.msdf_pixel_range;
                glyph.sampler_preset = renderer::quad_sampler_preset_t::smooth_clamp;

                if (target == debug_text_target_t::log_console)
                    g_renderer->draw_log_console_text_quad(glyph);
                else
                    g_renderer->draw_overlay_text_quad(glyph);
            }
        }
    } // namespace

    void text(const float x, const float y, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, 0xFFFFFFFFu, debug_text_target_t::overlay, fmt, args);
        va_end(args);
    }

    void text_colored(const float x, const float y, const uint32_t color, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, color, debug_text_target_t::overlay, fmt, args);
        va_end(args);
    }

    void log_console_text(const float x, const float y, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, 0xFFFFFFFFu, debug_text_target_t::log_console, fmt, args);
        va_end(args);
    }

    void log_console_text_colored(const float x, const float y, const uint32_t color, const char* fmt, ...) noexcept
    {
        va_list args;
        va_start(args, fmt);
        text_v(x, y, color, debug_text_target_t::log_console, fmt, args);
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
