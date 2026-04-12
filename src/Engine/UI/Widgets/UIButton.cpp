//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIButton.h"

#include "Renderer/Renderer.h"

namespace carrot::ui {
    ui_button_t::ui_button_t(std::string label) noexcept : _label(std::move(label))
    {
        set_focusable(true);
        set_desired_size({ 320.f, 48.f });
        set_min_size({ 200.f, 32.f });

        _label_widget = &emplace_child<ui_label_t>(_label);
        _label_widget->set_font_size(22.0f);
        _label_widget->set_horizontal_alignment(ui_label_horizontal_alignment_t::center);
        sync_label_style();
    }

    void ui_button_t::set_label(std::string label) noexcept
    {
        if (_label == label)
            return;

        _label = std::move(label);
        if (_label_widget)
            _label_widget->set_text(_label);
    }

    void ui_button_t::set_style(const ui_button_style_t& style) noexcept
    {
        _style = style;
        sync_label_style();
    }

    void ui_button_t::set_text_style(const ui_button_text_style_t& style) noexcept
    {
        _text_style = style;
        sync_label_style();
    }

    std::string_view ui_button_t::get_debug_name() const noexcept
    {
        return _label;
    }

    bool ui_button_t::get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept
    {
        (void)out;
        return false;
    }

    void ui_button_t::arrange_children(const ui_rect_t& bounds) noexcept
    {
        if (!_label_widget || _label_widget->is_collapsed())
            return;

        const ui_thickness_t& padding{ _style.content_padding };

        const ui_size_t& desired_size{ _label_widget->get_desired_size() };
        const float inner_width{ std::max(0.0f, bounds.width - padding.left - padding.right) };
        const float inner_height{ std::max(0.0f, bounds.height - padding.top - padding.bottom) };
        const float label_height{ std::min(desired_size.height, inner_height) };
        const float label_y{ bounds.y + padding.top + std::max(0.0f, inner_height - label_height) * 0.5f };

        _label_widget->layout_tree({
            .x = bounds.x + padding.left,
            .y = label_y,
            .width = inner_width,
            .height = label_height,
        });
    }

    void ui_button_t::on_render(renderer::renderer_t& renderer) const noexcept
    {
        const ui_rect_t& bounds{ get_layout_bounds() };
        if (bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const ui_button_surface_style_t& surface_style{ resolve_surface_style() };
        const float border_thickness{ std::max(0.0f, _style.border_thickness) };

        renderer.draw_ui_solid_quad({
            .x = bounds.x - border_thickness,
            .y = bounds.y - border_thickness,
            .width = bounds.width + (2.0f * border_thickness),
            .height = bounds.height + (2.0f * border_thickness),
            .layer = renderer::render_layer_t::ui,
            .color = surface_style.border_color,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        renderer.draw_ui_solid_quad({
            .x = bounds.x,
            .y = bounds.y,
            .width = bounds.width,
            .height = bounds.height,
            .layer = renderer::render_layer_t::ui,
            .color = surface_style.fill_color,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });
    }

    void ui_button_t::on_focus_gained() noexcept
    {
        _is_focused = true;
        sync_label_style();
        if (_on_focused)
            _on_focused();
    }

    void ui_button_t::on_focus_lost() noexcept
    {
        _is_focused = false;
        sync_label_style();
        if (_on_focus_lost)
            _on_focus_lost();
    }

    bool ui_button_t::on_ui_accept() noexcept
    {
        if (_on_pressed)
            _on_pressed();

        return true;
    }

    bool ui_button_t::on_ui_cancel() noexcept
    {
        if (_on_canceled)
            _on_canceled();

        return true;
    }

    const ui_button_surface_style_t& ui_button_t::resolve_surface_style() const noexcept
    {
        if (!is_enabled())
            return _style.disabled;

        return _is_focused ? _style.focused : _style.normal;
    }

    uint32_t ui_button_t::resolve_label_color() const noexcept
    {
        if (!is_enabled())
            return _style.disabled_label_color;

        return _is_focused ? _style.focused_label_color : _style.label_color;
    }

    void ui_button_t::sync_label_style() noexcept
    {
        if (!_label_widget) return;

        _label_widget->set_text(_label);
        _label_widget->set_font_asset_id(_text_style.font_asset_id);
        _label_widget->set_font_size(_text_style.font_size);
        _label_widget->set_horizontal_alignment(_text_style.horizontal_alignment);
        _label_widget->set_wrap_width(_text_style.wrap_width);
        _label_widget->set_color(resolve_label_color());
    }
} // namespace carrot::ui
