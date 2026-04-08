//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UIWidget.h"

namespace carrot::ui {
    class ui_root_widget_t final : public ui_widget_t
    {
    public:
        ui_root_widget_t() = default;
        ~ui_root_widget_t() override = default;

        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_root_widget_t"; }
    };
} // namespace carrot::ui
