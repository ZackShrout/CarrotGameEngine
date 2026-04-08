//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Module.h"
#include "UIRootWidget.h"

#include <memory>

namespace carrot::ui {
    class ui_widget_t;

    class ui_module_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("UI")

        ui_module_t() = default;
        ~ui_module_t() override = default;

        void init() override;
        void shutdown() override;

        void update(float delta_time) noexcept override;

        [[nodiscard]] ui_root_widget_t* get_root() noexcept { return _root.get(); }
        [[nodiscard]] const ui_root_widget_t* get_root() const noexcept { return _root.get(); }

        void set_root(std::unique_ptr<ui_root_widget_t> root) noexcept;
        void clear_root() noexcept;

    private:
        std::unique_ptr<ui_root_widget_t> _root;
    };
} // namespace carrot::ui
