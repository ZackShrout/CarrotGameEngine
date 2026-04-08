//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIService.h"

#include "UIModule.h"

namespace carrot::ui {
    void ui_service_t::provide(ui_module_t* instance) noexcept
    {
        // We still overwrite — last write wins (helps hot-reload experiments)
        if (_instance != nullptr) [[unlikely]]
                LOG_UI_WARN("UI service already provided — double registration?");

        _instance = instance;
    }

    ui_module_t& ui_service_t::get()
    {
        if (_instance == nullptr) [[unlikely]]
                LOG_UI_FATAL("Attempted to use ui_service_t::get() before ui_module_t was provided");

        return *_instance;
    }
} // namespace carrot::ui
