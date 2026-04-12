//
// Created by Zack Shrout on 4/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UILabel.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetService.h"
#include "Assets/Font/TextLayout.h"
#include "Renderer/Renderer.h"

namespace carrot::ui {
    namespace {
        [[nodiscard]] const assets::loaded_font_asset_t* try_resolve_font(const std::string_view font_asset_id) noexcept
        {
            assets::asset_manager_t* asset_manager{ assets::asset_service_t::try_manager() };
            if (!asset_manager)
                return nullptr;

            const assets::loaded_font_asset_t* font{ asset_manager->fonts().get(font_asset_id) };
            if (!font || !font->valid())
                return nullptr;

            return font;
        }
    } // namespace

    ui_label_t::ui_label_t(std::string text) noexcept
        : _text(std::move(text))
    {
        refresh_desired_size_if_needed();
    }

    void ui_label_t::set_text(std::string text) noexcept
    {
        if (_text == text)
            return;

        _text = std::move(text);
        mark_measured_size_dirty();
    }

    void ui_label_t::set_font_asset_id(std::string font_asset_id) noexcept
    {
        if (_font_asset_id == font_asset_id)
            return;

        _font_asset_id = std::move(font_asset_id);
        mark_measured_size_dirty();
    }

    void ui_label_t::set_font_size(const float font_size) noexcept
    {
        const float clamped_font_size{ std::max(0.0f, font_size) };
        if (_font_size == clamped_font_size)
            return;

        _font_size = clamped_font_size;
        mark_measured_size_dirty();
    }

    void ui_label_t::set_wrap_width(const float wrap_width) noexcept
    {
        const float clamped_wrap_width{ std::max(0.0f, wrap_width) };
        if (_wrap_width == clamped_wrap_width)
            return;

        _wrap_width = clamped_wrap_width;
        mark_measured_size_dirty();
    }

    void ui_label_t::on_tick([[maybe_unused]] const float delta_time) noexcept
    {
        refresh_desired_size_if_needed();
    }

    void ui_label_t::on_attached_to_tree() noexcept
    {
        refresh_desired_size_if_needed();
    }

    void ui_label_t::on_render(renderer::renderer_t& renderer) const noexcept
    {
        if (_text.empty())
            return;

        const assets::loaded_font_asset_t* font{ try_resolve_font(_font_asset_id) };
        if (!font)
            return;

        const ui_rect_t& bounds{ get_layout_bounds() };
        if (bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const float effective_wrap_width{ _wrap_width > 0.0f ? std::min(_wrap_width, bounds.width) : 0.0f };
        const assets::text_layout_result_t layout{
            assets::layout_text(*font,
                                _text,
                                assets::text_layout_settings_t{
                                    .font_size = _font_size,
                                    .wrap_width = effective_wrap_width,
                                })
        };

        float text_origin_x{ bounds.x };
        switch (_horizontal_alignment)
        {
            case ui_label_horizontal_alignment_t::center:
                text_origin_x = bounds.x + std::max(0.0f, bounds.width - layout.bounds.width) * 0.5f;
                break;
            case ui_label_horizontal_alignment_t::end:
                text_origin_x = bounds.x + std::max(0.0f, bounds.width - layout.bounds.width);
                break;
            case ui_label_horizontal_alignment_t::start:
            default:
                text_origin_x = bounds.x;
                break;
        }

        for (const assets::positioned_glyph_t& positioned : layout.glyphs)
        {
            if (!positioned.glyph || positioned.width <= 0.0f || positioned.height <= 0.0f)
                continue;

            renderer.draw_ui_text_quad(renderer::textured_quad_draw_info_t{
                .texture = font->atlas_texture.get(),
                .x = text_origin_x + positioned.x,
                .y = bounds.y + positioned.y,
                .width = positioned.width,
                .height = positioned.height,
                .u0 = positioned.u0,
                .v0 = positioned.v0,
                .u1 = positioned.u1,
                .v1 = positioned.v1,
                .layer = renderer::render_layer_t::ui,
                .color = _color,
                .effect_param0 = font->cooked.metrics.msdf_pixel_range,
                .sampler_preset = renderer::quad_sampler_preset_t::smooth_clamp,
            });
        }
    }

    void ui_label_t::mark_measured_size_dirty() noexcept
    {
        _desired_size_dirty = true;
        invalidate_layout();
    }

    void ui_label_t::refresh_desired_size_if_needed() noexcept
    {
        if (!_desired_size_dirty)
            return;

        _desired_size_dirty = false;

        const assets::loaded_font_asset_t* font{ try_resolve_font(_font_asset_id) };
        if (!font)
            return;

        const assets::text_layout_bounds_t measured{
            assets::measure_text(*font,
                                 _text,
                                 assets::text_layout_settings_t{
                                     .font_size = _font_size,
                                     .wrap_width = _wrap_width,
                                 })
        };

        set_desired_size({
            .width = measured.width,
            .height = measured.height
        });
    }
} // namespace carrot::ui
