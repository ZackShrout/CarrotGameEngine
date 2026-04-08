//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/CoreDefines.h"

#include <concepts>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::ui {
    using ui_widget_id_t = uint64_t;
    using ui_widget_flags_t = uint32_t;

    enum class ui_widget_visibility_t : uint8_t
    {
        visible,
        hidden,
        collapsed,
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

        [[nodiscard]] bool is_enabled() const noexcept;
        [[nodiscard]] bool is_focusable() const noexcept;
        [[nodiscard]] bool is_visible() const noexcept;
        [[nodiscard]] bool is_hidden() const noexcept;
        [[nodiscard]] bool is_collapsed() const noexcept;

        void set_enabled(bool enabled) noexcept;
        void set_focusable(bool focusable) noexcept;
        void set_visibility(ui_widget_visibility_t visibility) noexcept;

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

        void attach_to_tree() noexcept;
        void detach_from_tree() noexcept;

        [[nodiscard]] virtual std::string_view get_debug_name() const noexcept = 0;

    protected:
        virtual void on_tick(const float delta_time) noexcept { (void)delta_time; }

        virtual void on_child_added(ui_widget_t& child) noexcept { (void)child; }
        virtual void on_child_removed(ui_widget_t& child) noexcept { (void)child; }

        virtual void on_attached_to_tree() noexcept {}
        virtual void on_detached_from_tree() noexcept {}

    private:
        [[nodiscard]] bool has_flag(ui_widget_flag_bits_t flag) const noexcept;
        void set_flag(ui_widget_flag_bits_t flag, bool enabled) noexcept;

        void attach_subtree() noexcept;
        void detach_subtree() noexcept;

        static ui_widget_id_t generate_next_id() noexcept;

        ui_widget_id_t _id{ 0 };
        ui_widget_t* _parent{ nullptr };
        std::vector<std::unique_ptr<ui_widget_t>> _children;

        ui_widget_visibility_t _visibility{ ui_widget_visibility_t::visible };
        ui_widget_flags_t _flags{ static_cast<ui_widget_flags_t>(ui_widget_flag_bits_t::enabled) };

        bool _is_attached{ false };
    };
} // namespace carrot::ui