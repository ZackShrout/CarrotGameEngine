//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "KeyCodes.h"

namespace carrot::input {
    [[nodiscard]] key_code to_carrot_key(uint32_t platform_code) noexcept;
    [[nodiscard]] mouse_button to_carrot_mouse_button(uint32_t platform_button) noexcept;
} // namespace carrot::input
