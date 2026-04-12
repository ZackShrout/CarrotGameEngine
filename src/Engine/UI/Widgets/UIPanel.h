//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

#include <string>

namespace carrot::ui {
    struct ui_panel_style_t
    {
        uint32_t fill_color{ 0xCC171A26u };
        uint32_t border_color{ 0xFF5B647Au };

        // Reserved for future skinned/textured panels. Color-only rendering is used for now.
        std::string texture_asset_id;

        float border_thickness{ 2.0f };
        ui_thickness_t padding{ 18.f, 16.f, 18.f, 16.f };
    };

    class ui_panel_t final : public ui_widget_t
    {
    public:
        [[nodiscard]] const ui_panel_style_t& get_style() const noexcept { return _style; }
        void set_style(const ui_panel_style_t& style) noexcept;

        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_panel_t"; }

    protected:
        void arrange_children(const ui_rect_t& bounds) noexcept override;
        void on_render(renderer::renderer_t& renderer) const noexcept override;

    private:
        ui_panel_style_t _style{ };
    };
} // namespace carrot::ui
