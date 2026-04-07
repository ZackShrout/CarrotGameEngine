//
// Created by Codex on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>
#include <string_view>

namespace carrot::utils::text {
    [[nodiscard]] std::wstring utf8_to_wide(std::string_view utf8) noexcept;
    [[nodiscard]] std::string wide_to_utf8(std::wstring_view wide) noexcept;
} // namespace carrot::utils::text

