//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/CoreDefines.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::ui {
    using ui_widget_id_t = uint64_t;
    using ui_widget_flags_t = uint32_t;

    struct ui_size_t
    {
        float width{ 0.f };
        float height{ 0.f };
    };

    struct ui_rect_t
    {
        float x{ 0.f };
        float y{ 0.f };
        float width{ 0.f };
        float height{ 0.f };
    };

    struct ui_thickness_t
    {
        float left{ 0.f };
        float top{ 0.f };
        float right{ 0.f };
        float bottom{ 0.f };
    };

    struct ui_debug_visual_style_t
    {
        uint32_t fill_color{ 0x331A1A1Au };
        uint32_t focused_fill_color{ 0x88008CFFu };
        uint32_t border_color{ 0xFF7A7A7Au };
        uint32_t focused_border_color{ 0xFF00D9FFu };
        float border_thickness{ 2.f };
    };

    enum class ui_widget_visibility_t : uint8_t
    {
        visible,
        hidden,
        collapsed,
    };

    enum class ui_main_axis_size_rule_t : uint8_t
    {
        desired,
        flex,
    };

    enum class ui_navigation_direction_t : uint8_t
    {
        up = 0,
        down = 1,
        left = 2,
        right = 3,
    };

    enum class ui_widget_flag_bits_t : uint32_t
    {
        none = 0,
        enabled = 1u << 0,
        focusable = 1u << 1,
        clip_children = 1u << 2,
    };

    [[nodiscard]] constexpr ui_widget_flags_t operator|(const ui_widget_flag_bits_t lhs, const ui_widget_flag_bits_t rhs) noexcept
    {
        return static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs);
    }

    [[nodiscard]] constexpr ui_widget_flags_t operator|(const ui_widget_flags_t lhs, const ui_widget_flag_bits_t rhs) noexcept
    {
        return lhs | static_cast<uint32_t>(rhs);
    }

    class ui_widget_t
    {
    public:
        ui_widget_t();
        virtual ~ui_widget_t() = default;

        DISABLE_COPY_AND_MOVE(ui_widget_t);

        [[nodiscard]] ui_widget_id_t get_id() const noexcept { return _id; }

        [[nodiscard]] ui_widget_t* get_parent() noexcept { return _parent; }
        [[nodiscard]] const ui_widget_t* get_parent() const noexcept { return _parent; }

        [[nodiscard]] const std::vector<std::unique_ptr<ui_widget_t>>& get_children() const noexcept;
        [[nodiscard]] const ui_rect_t& get_layout_bounds() const noexcept { return _layout_bounds; }
        [[nodiscard]] const ui_size_t& get_desired_size() const noexcept { return _desired_size; }
        [[nodiscard]] const ui_size_t& get_min_size() const noexcept { return _min_size; }
        [[nodiscard]] const ui_size_t& get_max_size() const noexcept { return _max_size; }
        [[nodiscard]] bool is_layout_dirty() const noexcept { return _layout_dirty; }
        [[nodiscard]] ui_main_axis_size_rule_t get_main_axis_size_rule() const noexcept { return _main_axis_size_rule; }
        [[nodiscard]] float get_flex_weight() const noexcept { return _flex_weight; }

        [[nodiscard]] bool is_enabled() const noexcept;
        [[nodiscard]] bool is_focusable() const noexcept;
        [[nodiscard]] bool is_visible() const noexcept;
        [[nodiscard]] bool is_hidden() const noexcept;
        [[nodiscard]] bool is_collapsed() const noexcept;
        [[nodiscard]] bool can_receive_focus() const noexcept;
        [[nodiscard]] ui_widget_t* get_navigation_target(ui_navigation_direction_t direction) noexcept;
        [[nodiscard]] const ui_widget_t* get_navigation_target(ui_navigation_direction_t direction) const noexcept;
        void notify_focus_gained() noexcept { on_focus_gained(); }
        void notify_focus_lost() noexcept { on_focus_lost(); }
        [[nodiscard]] bool dispatch_ui_accept() noexcept { return on_ui_accept(); }
        [[nodiscard]] bool dispatch_ui_cancel() noexcept { return on_ui_cancel(); }
        [[nodiscard]] virtual bool get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept;

        void set_enabled(bool enabled) noexcept;
        void set_focusable(bool focusable) noexcept;
        void set_visibility(ui_widget_visibility_t visibility) noexcept;
        void set_desired_size(ui_size_t size) noexcept;
        void set_min_size(ui_size_t size) noexcept;
        void set_max_size(ui_size_t size) noexcept;
        void set_main_axis_size_rule(ui_main_axis_size_rule_t rule) noexcept;
        void set_flex_weight(float weight) noexcept;
        void set_navigation_target(ui_navigation_direction_t direction, ui_widget_t* target) noexcept;
        void clear_navigation_target(ui_navigation_direction_t direction) noexcept;
        void invalidate_layout() noexcept;

        void add_child(std::unique_ptr<ui_widget_t> child) noexcept;
        [[nodiscard]] std::unique_ptr<ui_widget_t> remove_child(ui_widget_t& child) noexcept;

        void remove_all_children() noexcept;

        template<typename T, typename... Args>
        requires std::derived_from<T, ui_widget_t>
        T& emplace_child(Args&&... args)
        {
            auto child{ std::make_unique<T>(std::forward<Args>(args)...) };
            T* raw_child{ child.get() };

            add_child(std::move(child));

            return *raw_child;
        }

        void tick_tree(float delta_time) noexcept;
        void layout_tree(const ui_rect_t& bounds) noexcept;
        void render_tree(renderer::renderer_t& renderer) const noexcept;

        void attach_to_tree() noexcept;
        void detach_from_tree() noexcept;

        [[nodiscard]] virtual std::string_view get_debug_name() const noexcept = 0;

    protected:
        virtual void on_tick(const float delta_time) noexcept { (void)delta_time; }

        virtual void on_child_added(ui_widget_t& child) noexcept { (void)child; }
        virtual void on_child_removed(ui_widget_t& child) noexcept { (void)child; }

        virtual void on_attached_to_tree() noexcept {}
        virtual void on_detached_from_tree() noexcept {}
        virtual void on_layout_updated(const ui_rect_t& bounds) noexcept { (void)bounds; }
        virtual void on_render(renderer::renderer_t& renderer) const noexcept { (void)renderer; }
        virtual void arrange_children(const ui_rect_t& bounds) noexcept;
        virtual void on_focus_gained() noexcept {}
        virtual void on_focus_lost() noexcept {}
        [[nodiscard]] virtual bool on_ui_accept() noexcept { return false; }
        [[nodiscard]] virtual bool on_ui_cancel() noexcept { return false; }

    private:
        [[nodiscard]] bool has_flag(ui_widget_flag_bits_t flag) const noexcept;
        void set_flag(ui_widget_flag_bits_t flag, bool enabled) noexcept;

        void layout_subtree(const ui_rect_t& bounds, bool parent_layout_dirty) noexcept;
        void attach_subtree() noexcept;
        void detach_subtree() noexcept;

        static ui_widget_id_t generate_next_id() noexcept;

        ui_widget_id_t _id{ 0 };
        ui_widget_t* _parent{ nullptr };
        std::vector<std::unique_ptr<ui_widget_t>> _children;

        ui_widget_visibility_t _visibility{ ui_widget_visibility_t::visible };
        ui_widget_flags_t _flags{ static_cast<ui_widget_flags_t>(ui_widget_flag_bits_t::enabled) };
        ui_size_t _desired_size{ 0.f, 0.f };
        ui_size_t _min_size{ 0.f, 0.f };
        ui_size_t _max_size{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        ui_rect_t _layout_bounds{ 0.f, 0.f, 0.f, 0.f };
        ui_main_axis_size_rule_t _main_axis_size_rule{ ui_main_axis_size_rule_t::desired };
        float _flex_weight{ 1.f };
        ui_widget_t* _navigation_targets[4]{ nullptr, nullptr, nullptr, nullptr };

        bool _layout_dirty{ true };
        bool _is_attached{ false };
    };
} // namespace carrot::ui
