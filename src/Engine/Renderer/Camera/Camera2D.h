//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    /**
     * @brief Minimal 2D camera for orthographic world rendering.
     *
     * V1 semantics:
     * - position is the world-space top-left corner of the visible region
     * - zoom of 1.0 means 1 world unit maps to 1 screen unit
     * - viewport_size is the render area size in pixels/units
     *
     * This camera is intentionally small and renderer-facing for the first
     * camera/projection slice. It is not yet tied to world objects/components.
     */
    struct camera_2d_t
    {
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 viewport_size{ 1280.f, 720.f };
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
         * @brief Builds the orthographic projection matrix.
         *
         * Uses a top-left origin, with +X right and +Y down, which is convenient
         * for 2D sprite and quad rendering.
         *
         * @return Projection matrix.
         */
        [[nodiscard]] chlm::float4x4 projection_matrix() const noexcept
        {
            const float safe_zoom{ zoom <= 0.f ? 1.f : zoom };
            const float visible_width{ viewport_size.x / safe_zoom };
            const float visible_height{ viewport_size.y / safe_zoom };

            return chlm::float4x4::ortho_off_center_lh_top_left(0.f, visible_width, 0.f, visible_height, z_near, z_far);
        }

        /**
         * @brief Builds the combined view-projection matrix.
         *
         * @return View-projection matrix.
         */
        [[nodiscard]] chlm::float4x4 view_projection_matrix() const noexcept
        {
            return projection_matrix() * view_matrix();
        }
    };
} // namespace carrot::renderer
