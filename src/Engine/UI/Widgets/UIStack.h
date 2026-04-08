//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

namespace carrot::ui {
    enum class ui_stack_orientation_t : uint8_t
    {
        horizontal,
        vertical,
    };

    enum class ui_stack_cross_alignment_t : uint8_t
    {
        start,
        center,
        end,
        stretch,
    };

    class ui_stack_t final : public ui_widget_t
    {
    public:
        explicit ui_stack_t(ui_stack_orientation_t orientation = ui_stack_orientation_t::vertical) noexcept
            : _orientation(orientation)
        {
        }

        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_stack_t"; }

        [[nodiscard]] ui_stack_orientation_t get_orientation() const noexcept { return _orientation; }
        void set_orientation(ui_stack_orientation_t orientation) noexcept;

        [[nodiscard]] ui_stack_cross_alignment_t get_cross_alignment() const noexcept { return _cross_alignment; }
        void set_cross_alignment(ui_stack_cross_alignment_t alignment) noexcept;

        [[nodiscard]] float get_spacing() const noexcept { return _spacing; }
        void set_spacing(float spacing) noexcept;

        [[nodiscard]] const ui_thickness_t& get_padding() const noexcept { return _padding; }
        void set_padding(const ui_thickness_t& padding) noexcept;

    protected:
        void arrange_children(const ui_rect_t& bounds) noexcept override;

    private:
        ui_stack_orientation_t _orientation{ ui_stack_orientation_t::vertical };
        ui_stack_cross_alignment_t _cross_alignment{ ui_stack_cross_alignment_t::stretch };
        float _spacing{ 0.f };
        ui_thickness_t _padding{ };
    };
} // namespace carrot::ui
