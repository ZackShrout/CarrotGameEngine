//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIAssetPreview.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetService.h"
#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Texture/TextureAsset.h"
#include "Renderer/Renderer.h"

namespace carrot::ui {
    namespace {
        [[nodiscard]] renderer::uv_rect_t sprite_frame_uv_rect(const assets::sprite_frame_t& frame,
                                                               const rhi::rhi_texture_t& texture) noexcept
        {
            const float texture_width{ static_cast<float>(std::max(1u, texture.width())) };
            const float texture_height{ static_cast<float>(std::max(1u, texture.height())) };

            const float x0{ static_cast<float>(frame.pixel_rect.position.x) / texture_width };
            const float y0{ static_cast<float>(frame.pixel_rect.position.y) / texture_height };
            const float x1{ static_cast<float>(frame.pixel_rect.position.x + frame.pixel_rect.size.x) / texture_width };
            const float y1{ static_cast<float>(frame.pixel_rect.position.y + frame.pixel_rect.size.y) / texture_height };

            return {
                .u_min = x0,
                .v_min = y0,
                .u_max = x1,
                .v_max = y1
            };
        }
    } // namespace

    ui_asset_preview_t::ui_asset_preview_t() noexcept
    {
        set_desired_size({ 420.f, 260.f });
        set_min_size({ 320.f, 220.f });
    }

    void ui_asset_preview_t::clear_preview() noexcept
    {
        _preview_kind = ui_asset_preview_kind_t::none;
        _preview_asset_id.clear();
    }

    void ui_asset_preview_t::set_texture_asset_id(std::string logical_id) noexcept
    {
        _preview_kind = ui_asset_preview_kind_t::texture;
        _preview_asset_id = std::move(logical_id);
    }

    void ui_asset_preview_t::set_sprite_asset_id(std::string logical_id) noexcept
    {
        _preview_kind = ui_asset_preview_kind_t::sprite;
        _preview_asset_id = std::move(logical_id);
    }

    void ui_asset_preview_t::set_style(const ui_asset_preview_style_t& style) noexcept
    {
        _style = style;
    }

    void ui_asset_preview_t::on_render(renderer::renderer_t& renderer) const noexcept
    {
        const ui_rect_t& bounds{ get_layout_bounds() };
        if (bounds.width <= 0.f || bounds.height <= 0.f)
            return;

        const float border{ std::max(0.f, _style.border_thickness) };
        renderer.draw_ui_solid_quad({
            .x = bounds.x - border,
            .y = bounds.y - border,
            .width = bounds.width + (2.f * border),
            .height = bounds.height + (2.f * border),
            .layer = renderer::render_layer_t::ui,
            .color = _style.frame_color,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        renderer.draw_ui_solid_quad({
            .x = bounds.x,
            .y = bounds.y,
            .width = bounds.width,
            .height = bounds.height,
            .layer = renderer::render_layer_t::ui,
            .color = _style.background_color,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });

        const float padding{ std::max(0.f, _style.padding) };
        const float inner_width{ std::max(0.f, bounds.width - 2.f * padding) };
        const float inner_height{ std::max(0.f, bounds.height - 2.f * padding) };
        if (inner_width <= 0.f || inner_height <= 0.f)
            return;

        const rhi::rhi_texture_t* texture{ nullptr };
        renderer::uv_rect_t uv_rect{ };
        float aspect_ratio{ 1.f };
        if (!resolve_preview_quad(texture, uv_rect, aspect_ratio) || !texture)
            return;

        const float safe_aspect{ std::max(0.001f, aspect_ratio) };
        float draw_width{ inner_width };
        float draw_height{ draw_width / safe_aspect };
        if (draw_height > inner_height)
        {
            draw_height = inner_height;
            draw_width = draw_height * safe_aspect;
        }

        const float draw_x{ bounds.x + padding + (inner_width - draw_width) * 0.5f };
        const float draw_y{ bounds.y + padding + (inner_height - draw_height) * 0.5f };

        renderer.draw_ui_textured_quad({
            .texture = texture,
            .x = draw_x,
            .y = draw_y,
            .width = draw_width,
            .height = draw_height,
            .u0 = uv_rect.u_min,
            .v0 = uv_rect.v_min,
            .u1 = uv_rect.u_max,
            .v1 = uv_rect.v_max,
            .layer = renderer::render_layer_t::ui,
            .color = _style.image_tint,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
        });
    }

    bool ui_asset_preview_t::resolve_preview_quad(const rhi::rhi_texture_t*& out_texture,
                                                  renderer::uv_rect_t& out_uv_rect,
                                                  float& out_aspect_ratio) const noexcept
    {
        out_texture = nullptr;
        out_uv_rect = { };
        out_aspect_ratio = 1.f;

        if (_preview_kind == ui_asset_preview_kind_t::none || _preview_asset_id.empty())
            return false;

        assets::asset_manager_t* asset_manager{ assets::asset_service_t::try_manager() };
        if (!asset_manager)
            return false;

        switch (_preview_kind)
        {
            case ui_asset_preview_kind_t::texture:
            {
                const assets::loaded_texture_asset_t* texture_asset{ asset_manager->textures().get(_preview_asset_id) };
                if (!texture_asset || !texture_asset->valid() || !texture_asset->texture)
                    return false;

                out_texture = texture_asset->texture.get();
                out_aspect_ratio = static_cast<float>(std::max(1u, out_texture->width()))
                                 / static_cast<float>(std::max(1u, out_texture->height()));
                return true;
            }

            case ui_asset_preview_kind_t::sprite:
            {
                const assets::loaded_sprite_asset_t* sprite_asset{ asset_manager->sprites().get(_preview_asset_id) };
                if (!sprite_asset || !sprite_asset->valid() || !sprite_asset->texture() || !sprite_asset->texture()->texture)
                    return false;

                const assets::sprite_frame_t* frame{ sprite_asset->sprite().frame_at(0u) };
                if (!frame || frame->pixel_rect.size.x == 0u || frame->pixel_rect.size.y == 0u)
                    return false;

                out_texture = sprite_asset->texture()->texture.get();
                out_uv_rect = sprite_frame_uv_rect(*frame, *out_texture);
                out_aspect_ratio = static_cast<float>(frame->pixel_rect.size.x)
                                 / static_cast<float>(frame->pixel_rect.size.y);
                return true;
            }

            case ui_asset_preview_kind_t::none:
            default:
                return false;
        }
    }
} // namespace carrot::ui
