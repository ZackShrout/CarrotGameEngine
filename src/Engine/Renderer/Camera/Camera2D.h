//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    enum class camera_2d_sizing_mode_t
    {
        responsive_world_view,
        fixed_aspect_letterbox,
        fixed_height,
        fixed_width
    };

    [[nodiscard]] inline const char* camera_2d_sizing_mode_to_string(const camera_2d_sizing_mode_t mode) noexcept
    {
        switch (mode)
        {
            case camera_2d_sizing_mode_t::responsive_world_view: return "responsive_world_view";
            case camera_2d_sizing_mode_t::fixed_aspect_letterbox: return "fixed_aspect_letterbox";
            case camera_2d_sizing_mode_t::fixed_height: return "fixed_height";
            case camera_2d_sizing_mode_t::fixed_width: return "fixed_width";
        }

        return "unknown";
    }

    struct resolved_camera_2d_t
    {
        chlm::float2 visible_world_size{ 1280.f, 720.f };
        chlm::uint_rect viewport_rect_px{ .position = { 0u, 0u }, .size = { 1280u, 720u } };
        float z_near{ 0.f };
        float z_far{ 1.f };

        [[nodiscard]] chlm::float4x4 projection_matrix() const noexcept
        {
            return chlm::float4x4::ortho_off_center_lh_top_left(0.f,
                                                                visible_world_size.x,
                                                                0.f,
                                                                visible_world_size.y,
                                                                z_near,
                                                                z_far);
        }
    };

    /**
     * @brief Minimal 2D camera for orthographic world rendering.
     *
     * V2 semantics:
     * - position is the world-space top-left corner of the visible region
     * - zoom of 1.0 means 1 world unit maps to 1 screen unit
     * - sizing_mode controls how render-target resizing affects framing
     * - design_view_size is the authored world-space framing at zoom 1.0
     *
     * This camera is intentionally small and renderer-facing for the first
     * camera/projection slice. It is not yet tied to world objects/components.
     */
    struct camera_2d_t
    {
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 design_view_size{ 1280.f, 720.f };
        camera_2d_sizing_mode_t sizing_mode{ camera_2d_sizing_mode_t::responsive_world_view };
        float zoom{ 1.f };
        float z_near{ 0.f };
        float z_far{ 1.f };

        /**
         * @brief Builds the view matrix.
         *
         * The camera position represents the world-space top-left of the visible
         * region, so the view matrix translates the world by -position.
         *
         * @return View matrix.
         */
        [[nodiscard]] chlm::float4x4 view_matrix() const noexcept
        {
            return chlm::float4x4::translate({ -position.x, -position.y, 0.f });
        }

        /**
         * @brief Resolves the visible world size and presentation viewport.
         *
         * The returned viewport rect is in render-target pixels and may be smaller
         * than the full render target when letterboxing/pillarboxing is active.
         */
        [[nodiscard]] resolved_camera_2d_t resolve(const chlm::uint2 render_target_size) const noexcept
        {
            const uint32_t safe_render_width{ render_target_size.x == 0u ? 1u : render_target_size.x };
            const uint32_t safe_render_height{ render_target_size.y == 0u ? 1u : render_target_size.y };
            const float safe_zoom{ zoom <= 0.f ? 1.f : zoom };
            const float safe_design_width{ design_view_size.x <= 0.f ? 1.f : design_view_size.x };
            const float safe_design_height{ design_view_size.y <= 0.f ? 1.f : design_view_size.y };
            const float render_aspect{ static_cast<float>(safe_render_width) / static_cast<float>(safe_render_height) };
            const float design_aspect{ safe_design_width / safe_design_height };

            resolved_camera_2d_t resolved{ };
            resolved.z_near = z_near;
            resolved.z_far = z_far;
            resolved.viewport_rect_px = {
                .position = { 0u, 0u },
                .size = { safe_render_width, safe_render_height }
            };

            switch (sizing_mode)
            {
                case camera_2d_sizing_mode_t::fixed_aspect_letterbox:
                {
                    resolved.visible_world_size = {
                        safe_design_width / safe_zoom,
                        safe_design_height / safe_zoom
                    };

                    uint32_t viewport_width{ safe_render_width };
                    uint32_t viewport_height{ static_cast<uint32_t>(static_cast<float>(viewport_width) / design_aspect) };

                    if (viewport_height > safe_render_height)
                    {
                        viewport_height = safe_render_height;
                        viewport_width = static_cast<uint32_t>(static_cast<float>(viewport_height) * design_aspect);
                    }

                    viewport_width = std::max(1u, viewport_width);
                    viewport_height = std::max(1u, viewport_height);

                    resolved.viewport_rect_px = {
                        .position = {
                            (safe_render_width - viewport_width) / 2u,
                            (safe_render_height - viewport_height) / 2u
                        },
                        .size = { viewport_width, viewport_height }
                    };
                    break;
                }

                case camera_2d_sizing_mode_t::fixed_height:
                {
                    const float visible_height{ safe_design_height / safe_zoom };
                    resolved.visible_world_size = {
                        visible_height * render_aspect,
                        visible_height
                    };
                    break;
                }

                case camera_2d_sizing_mode_t::fixed_width:
                {
                    const float visible_width{ safe_design_width / safe_zoom };
                    resolved.visible_world_size = {
                        visible_width,
                        visible_width / render_aspect
                    };
                    break;
                }

                case camera_2d_sizing_mode_t::responsive_world_view:
                {
                    resolved.visible_world_size = {
                        static_cast<float>(safe_render_width) / safe_zoom,
                        static_cast<float>(safe_render_height) / safe_zoom
                    };
                    break;
                }
            }

            return resolved;
        }

        /**
         * @brief Builds the orthographic projection matrix.
         */
        [[nodiscard]] chlm::float4x4 projection_matrix(const chlm::uint2 render_target_size) const noexcept
        {
            return resolve(render_target_size).projection_matrix();
        }

        /**
         * @brief Builds the combined view-projection matrix.
         */
        [[nodiscard]] chlm::float4x4 view_projection_matrix(const chlm::uint2 render_target_size) const noexcept
        {
            return projection_matrix(render_target_size) * view_matrix();
        }
    };
} // namespace carrot::renderer
