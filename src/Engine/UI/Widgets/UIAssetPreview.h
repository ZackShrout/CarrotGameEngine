//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

#include "Renderer/Draw/TexturedQuadTypes.h"

#include <string>
#include <string_view>

namespace carrot::rhi {
    class rhi_texture_t;
}

namespace carrot::ui {
    enum class ui_asset_preview_kind_t : uint8_t
    {
        none,
        texture,
        sprite,
    };

    struct ui_asset_preview_style_t
    {
        uint32_t background_color{ 0xFF22253Au };
        uint32_t frame_color{ 0xFF93B8D2u };
        uint32_t image_tint{ 0xFFFFFFFFu };
        float border_thickness{ 2.f };
        float padding{ 14.f };
    };

    class ui_asset_preview_t final : public ui_widget_t
    {
    public:
        ui_asset_preview_t() noexcept;

        void clear_preview() noexcept;
        void set_texture_asset_id(std::string logical_id) noexcept;
        void set_sprite_asset_id(std::string logical_id) noexcept;

        [[nodiscard]] ui_asset_preview_kind_t get_preview_kind() const noexcept { return _preview_kind; }
        [[nodiscard]] std::string_view get_preview_asset_id() const noexcept { return _preview_asset_id; }
        [[nodiscard]] const ui_asset_preview_style_t& get_style() const noexcept { return _style; }
        void set_style(const ui_asset_preview_style_t& style) noexcept;

        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_asset_preview_t"; }

    protected:
        void on_render(renderer::renderer_t& renderer) const noexcept override;

    private:
        [[nodiscard]] bool resolve_preview_quad(const rhi::rhi_texture_t*& out_texture,
                                                renderer::uv_rect_t& out_uv_rect,
                                                float& out_aspect_ratio) const noexcept;

        ui_asset_preview_kind_t _preview_kind{ ui_asset_preview_kind_t::none };
        std::string _preview_asset_id;
        ui_asset_preview_style_t _style{ };
    };
} // namespace carrot::ui
