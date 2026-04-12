//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"
#include "UILabel.h"

#include <functional>
#include <string>
#include <string_view>

namespace carrot::ui {
    struct ui_button_surface_style_t
    {
        uint32_t fill_color{ 0xFF3D1F1Fu };
        uint32_t border_color{ 0xFF7A7A7Au };

        // Reserved for future skinned/textured buttons. Color-only rendering is used for now.
        std::string texture_asset_id;
    };

    struct ui_button_text_style_t
    {
        std::string font_asset_id{ "font.engine.roboto_regular" };
        float font_size{ 24.0f };
        ui_label_horizontal_alignment_t horizontal_alignment{ ui_label_horizontal_alignment_t::center };
        float wrap_width{ 0.0f };
    };

    struct ui_button_style_t
    {
        ui_button_surface_style_t normal{ };
        ui_button_surface_style_t focused{
            .fill_color = 0xFF7A5230u,
            .border_color = 0xFFFFD86Bu
        };
        ui_button_surface_style_t disabled{
            .fill_color = 0xFF242424u,
            .border_color = 0xFF555555u
        };
        uint32_t label_color{ 0xFFF4F0E8u };
        uint32_t focused_label_color{ 0xFFFFF3C4u };
        uint32_t disabled_label_color{ 0xFF8F8F8Fu };
        float border_thickness{ 2.0f };
        ui_thickness_t content_padding{ 16.f, 8.f, 16.f, 8.f };
    };

    class ui_button_t final : public ui_widget_t
    {
    public:
        using callback_t = std::function<void()>;

        explicit ui_button_t(std::string label = "Button") noexcept;

        [[nodiscard]] std::string_view get_label() const noexcept { return _label; }
        void set_label(std::string label) noexcept;

        [[nodiscard]] const ui_button_style_t& get_style() const noexcept { return _style; }
        void set_style(const ui_button_style_t& style) noexcept;

        [[nodiscard]] const ui_button_text_style_t& get_text_style() const noexcept { return _text_style; }
        void set_text_style(const ui_button_text_style_t& style) noexcept;

        void set_on_focused(callback_t callback) noexcept { _on_focused = std::move(callback); }
        void set_on_focus_lost(callback_t callback) noexcept { _on_focus_lost = std::move(callback); }
        void set_on_pressed(callback_t callback) noexcept { _on_pressed = std::move(callback); }
        void set_on_canceled(callback_t callback) noexcept { _on_canceled = std::move(callback); }

        [[nodiscard]] std::string_view get_debug_name() const noexcept override;
        [[nodiscard]] bool get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept override;

    protected:
        void arrange_children(const ui_rect_t& bounds) noexcept override;
        void on_render(renderer::renderer_t& renderer) const noexcept override;
        void on_focus_gained() noexcept override;
        void on_focus_lost() noexcept override;
        [[nodiscard]] bool on_ui_accept() noexcept override;
        [[nodiscard]] bool on_ui_cancel() noexcept override;

    private:
        [[nodiscard]] const ui_button_surface_style_t& resolve_surface_style() const noexcept;
        [[nodiscard]] uint32_t resolve_label_color() const noexcept;
        void sync_label_style() noexcept;

        std::string _label;
        ui_label_t* _label_widget{ nullptr };
        bool _is_focused{ false };
        ui_button_style_t _style{ };
        ui_button_text_style_t _text_style{ };
        callback_t _on_focused;
        callback_t _on_focus_lost;
        callback_t _on_pressed;
        callback_t _on_canceled;
    };
} // namespace carrot::ui
