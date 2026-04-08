//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

namespace carrot::ui {
    class ui_panel_t final : public ui_widget_t
    {
    public:
        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_panel_t"; }
    };
} // namespace carrot::ui
