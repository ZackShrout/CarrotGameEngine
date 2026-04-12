//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIPanel.h"

#include "Renderer/Renderer.h"

namespace carrot::ui {
    void ui_panel_t::set_style(const ui_panel_style_t& style) noexcept
    {
        _style = style;
        invalidate_layout();
    }

    void ui_panel_t::arrange_children(const ui_rect_t& bounds) noexcept
    {
        const ui_thickness_t& padding{ _style.padding };
        const ui_rect_t inner_bounds{
            .x = bounds.x + padding.left,
            .y = bounds.y + padding.top,
            .width = std::max(0.0f, bounds.width - padding.left - padding.right),
            .height = std::max(0.0f, bounds.height - padding.top - padding.bottom),
        };

        for (const auto& child : get_children())
        {
            if (!child || child->is_collapsed())
                continue;

            child->layout_tree(inner_bounds);
        }
    }

    void ui_panel_t::on_render(renderer::renderer_t& renderer) const noexcept
    {
        const ui_rect_t& bounds{ get_layout_bounds() };
        if (bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const float border_thickness{ std::max(0.0f, _style.border_thickness) };
        if (border_thickness > 0.0f)
        {
            renderer.draw_ui_solid_quad({
                .x = bounds.x - border_thickness,
                .y = bounds.y - border_thickness,
                .width = bounds.width + (2.0f * border_thickness),
                .height = bounds.height + (2.0f * border_thickness),
                .layer = renderer::render_layer_t::ui,
                .color = _style.border_color,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });
        }

        renderer.draw_ui_solid_quad({
            .x = bounds.x,
            .y = bounds.y,
            .width = bounds.width,
            .height = bounds.height,
            .layer = renderer::render_layer_t::ui,
            .color = _style.fill_color,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });
    }

} // namespace carrot::ui
