//
// Created by Zack Shrout on 4/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

#include <string>
#include <string_view>

namespace carrot::ui {
    enum class ui_label_horizontal_alignment_t : uint8_t
    {
        start = 0,
        center,
        end,
    };

    class ui_label_t final : public ui_widget_t
    {
    public:
        explicit ui_label_t(std::string text = {}) noexcept;

        [[nodiscard]] std::string_view get_text() const noexcept { return _text; }
        void set_text(std::string text) noexcept;

        [[nodiscard]] std::string_view get_font_asset_id() const noexcept { return _font_asset_id; }
        void set_font_asset_id(std::string font_asset_id) noexcept;

        [[nodiscard]] float get_font_size() const noexcept { return _font_size; }
        void set_font_size(float font_size) noexcept;

        [[nodiscard]] float get_wrap_width() const noexcept { return _wrap_width; }
        void set_wrap_width(float wrap_width) noexcept;

        [[nodiscard]] uint32_t get_color() const noexcept { return _color; }
        void set_color(uint32_t color) noexcept { _color = color; }

        [[nodiscard]] ui_label_horizontal_alignment_t get_horizontal_alignment() const noexcept
        {
            return _horizontal_alignment;
        }
        void set_horizontal_alignment(ui_label_horizontal_alignment_t alignment) noexcept
        {
            _horizontal_alignment = alignment;
        }

        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return _text; }

    protected:
        void on_tick(float delta_time) noexcept override;
        void on_attached_to_tree() noexcept override;
        void on_render(renderer::renderer_t& renderer) const noexcept override;

    private:
        void mark_measured_size_dirty() noexcept;
        void refresh_desired_size_if_needed() noexcept;

        std::string _text;
        std::string _font_asset_id{ "font.engine.roboto_regular" };
        float _font_size{ 24.0f };
        float _wrap_width{ 0.0f };
        uint32_t _color{ 0xFFFFFFFFu };
        ui_label_horizontal_alignment_t _horizontal_alignment{ ui_label_horizontal_alignment_t::start };
        bool _desired_size_dirty{ true };
    };
} // namespace carrot::ui
