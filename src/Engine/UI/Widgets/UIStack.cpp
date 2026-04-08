//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIStack.h"

#include <algorithm>
#include <vector>

namespace carrot::ui {
    namespace {
        [[nodiscard]] float clamp_size(const float value, const float min_value, const float max_value) noexcept
        {
            return std::clamp(value, min_value, max_value);
        }
    } // anonymous namespace

    void ui_stack_t::set_orientation(const ui_stack_orientation_t orientation) noexcept
    {
        if (_orientation == orientation)
            return;

        _orientation = orientation;
        invalidate_layout();
    }

    void ui_stack_t::set_cross_alignment(const ui_stack_cross_alignment_t alignment) noexcept
    {
        if (_cross_alignment == alignment)
            return;

        _cross_alignment = alignment;
        invalidate_layout();
    }

    void ui_stack_t::set_spacing(const float spacing) noexcept
    {
        const float clamped_spacing{ std::max(0.f, spacing) };
        if (_spacing == clamped_spacing)
            return;

        _spacing = clamped_spacing;
        invalidate_layout();
    }

    void ui_stack_t::set_padding(const ui_thickness_t& padding) noexcept
    {
        const ui_thickness_t clamped_padding{
            .left = std::max(0.f, padding.left),
            .top = std::max(0.f, padding.top),
            .right = std::max(0.f, padding.right),
            .bottom = std::max(0.f, padding.bottom)
        };

        if (_padding.left == clamped_padding.left &&
            _padding.top == clamped_padding.top &&
            _padding.right == clamped_padding.right &&
            _padding.bottom == clamped_padding.bottom)
        {
            return;
        }

        _padding = clamped_padding;
        invalidate_layout();
    }

    void ui_stack_t::arrange_children(const ui_rect_t& bounds) noexcept
    {
        const float inner_x{ bounds.x + _padding.left };
        const float inner_y{ bounds.y + _padding.top };
        const float inner_width{ std::max(0.f, bounds.width - (_padding.left + _padding.right)) };
        const float inner_height{ std::max(0.f, bounds.height - (_padding.top + _padding.bottom)) };

        struct stack_child_layout_t
        {
            ui_widget_t* widget{ nullptr };
            bool is_flex{ false };
            float flex_weight{ 0.f };
            float min_main{ 0.f };
            float max_main{ 0.f };
            float desired_main{ 0.f };
            float min_cross{ 0.f };
            float max_cross{ 0.f };
            float desired_cross{ 0.f };
            float allocated_main{ 0.f };
        };

        constexpr float k_layout_epsilon{ 0.0001f };
        std::vector<stack_child_layout_t> children;
        children.reserve(get_children().size());
        for (const auto& child : get_children())
        {
            if (child->is_collapsed())
                continue;

            const ui_size_t desired{ child->get_desired_size() };
            const ui_size_t min_size{ child->get_min_size() };
            const ui_size_t max_size{ child->get_max_size() };

            const bool is_vertical{ _orientation == ui_stack_orientation_t::vertical };
            const float desired_main{ is_vertical ? std::max(0.f, desired.height) : std::max(0.f, desired.width) };
            const float min_main{ is_vertical ? min_size.height : min_size.width };
            const float max_main{ is_vertical ? max_size.height : max_size.width };
            const float desired_cross{ is_vertical ? std::max(0.f, desired.width) : std::max(0.f, desired.height) };
            const float min_cross{ is_vertical ? min_size.width : min_size.height };
            const float max_cross{ is_vertical ? max_size.width : max_size.height };

            children.emplace_back(stack_child_layout_t{
                .widget = child.get(),
                .is_flex = child->get_main_axis_size_rule() == ui_main_axis_size_rule_t::flex,
                .flex_weight = std::max(0.f, child->get_flex_weight()),
                .min_main = min_main,
                .max_main = std::max(min_main, max_main),
                .desired_main = desired_main,
                .min_cross = min_cross,
                .max_cross = std::max(min_cross, max_cross),
                .desired_cross = desired_cross,
                .allocated_main = 0.f
            });
        }

        if (children.empty())
            return;

        const float available_main{
            _orientation == ui_stack_orientation_t::vertical
                ? std::max(0.f, inner_height - (_spacing * static_cast<float>(children.size() - 1u)))
                : std::max(0.f, inner_width - (_spacing * static_cast<float>(children.size() - 1u)))
        };

        float total_fixed_main{ 0.f };
        float total_flex_min_main{ 0.f };
        std::vector<size_t> flex_indices;
        flex_indices.reserve(children.size());

        for (size_t i = 0; i < children.size(); ++i)
        {
            stack_child_layout_t& child{ children[i] };
            if (child.is_flex)
            {
                child.allocated_main = child.min_main;
                total_flex_min_main += child.min_main;
                flex_indices.push_back(i);
                continue;
            }

            child.allocated_main = clamp_size(child.desired_main, child.min_main, child.max_main);
            total_fixed_main += child.allocated_main;
        }

        // Min-overflow policy: minimum constraints are authoritative and may overflow the available axis.
        const float available_for_flex{ available_main - total_fixed_main };
        if (!flex_indices.empty() && available_for_flex > total_flex_min_main)
        {
            float distributable{ available_for_flex - total_flex_min_main };

            while (distributable > k_layout_epsilon)
            {
                std::vector<size_t> active_flex_indices;
                active_flex_indices.reserve(flex_indices.size());

                float total_weight{ 0.f };
                for (const size_t index : flex_indices)
                {
                    const stack_child_layout_t& child{ children[index] };
                    if (child.allocated_main >= child.max_main - k_layout_epsilon)
                        continue;

                    if (child.flex_weight > 0.f)
                        total_weight += child.flex_weight;

                    active_flex_indices.push_back(index);
                }

                if (active_flex_indices.empty())
                    break;

                const bool use_equal_distribution{ total_weight <= k_layout_epsilon };
                if (use_equal_distribution)
                    total_weight = static_cast<float>(active_flex_indices.size());

                float consumed{ 0.f };
                for (const size_t index : active_flex_indices)
                {
                    stack_child_layout_t& child{ children[index] };
                    const float weight{ use_equal_distribution ? 1.f : child.flex_weight };
                    if (weight <= 0.f)
                        continue;

                    const float share{ distributable * (weight / total_weight) };
                    const float capacity{ std::max(0.f, child.max_main - child.allocated_main) };
                    const float granted{ std::min(capacity, share) };
                    child.allocated_main += granted;
                    consumed += granted;
                }

                if (consumed <= k_layout_epsilon)
                    break;

                distributable -= consumed;
            }
        }

        float cursor{ _orientation == ui_stack_orientation_t::vertical ? inner_y : inner_x };
        bool placed_any{ false };

        for (stack_child_layout_t& child : children)
        {
            if (placed_any)
                cursor += _spacing;

            const float main_size{ child.allocated_main };

            ui_rect_t child_bounds{ };

            if (_orientation == ui_stack_orientation_t::vertical)
            {
                const float cross_size{
                    _cross_alignment == ui_stack_cross_alignment_t::stretch
                        ? clamp_size(inner_width, child.min_cross, child.max_cross)
                        : clamp_size(std::min(std::max(0.f, child.desired_cross), inner_width), child.min_cross, child.max_cross)
                };

                float child_x{ inner_x };
                if (_cross_alignment == ui_stack_cross_alignment_t::center)
                    child_x = inner_x + ((inner_width - cross_size) * 0.5f);
                else if (_cross_alignment == ui_stack_cross_alignment_t::end)
                    child_x = inner_x + (inner_width - cross_size);

                child_bounds = {
                    .x = child_x,
                    .y = cursor,
                    .width = cross_size,
                    .height = main_size
                };

                cursor += main_size;
            }
            else
            {
                const float cross_size{
                    _cross_alignment == ui_stack_cross_alignment_t::stretch
                        ? clamp_size(inner_height, child.min_cross, child.max_cross)
                        : clamp_size(std::min(std::max(0.f, child.desired_cross), inner_height), child.min_cross, child.max_cross)
                };

                float child_y{ inner_y };
                if (_cross_alignment == ui_stack_cross_alignment_t::center)
                    child_y = inner_y + ((inner_height - cross_size) * 0.5f);
                else if (_cross_alignment == ui_stack_cross_alignment_t::end)
                    child_y = inner_y + (inner_height - cross_size);

                child_bounds = {
                    .x = cursor,
                    .y = child_y,
                    .width = main_size,
                    .height = cross_size
                };

                cursor += main_size;
            }

            child.widget->layout_tree(child_bounds);
            placed_any = true;
        }
    }
} // namespace carrot::ui
