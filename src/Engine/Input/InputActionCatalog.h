//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "InputActionId.h"

#include <optional>
#include <span>
#include <string_view>

namespace carrot::input {
    struct input_action_definition_t
    {
        input_action_handle_t action{ };
        std::string_view display_name{ };
        std::string_view category{ };
    };

    class input_action_catalog_view_t
    {
    public:
        constexpr input_action_catalog_view_t() noexcept = default;
        constexpr explicit input_action_catalog_view_t(const std::span<const input_action_definition_t> definitions) noexcept
            : _definitions(definitions)
        {
        }

        [[nodiscard]] constexpr std::span<const input_action_definition_t> definitions() const noexcept { return _definitions; }
        [[nodiscard]] constexpr bool empty() const noexcept { return _definitions.empty(); }

        [[nodiscard]] constexpr std::optional<input_action_definition_t> find(const input_action_id_t action) const noexcept
        {
            for (const input_action_definition_t& definition : _definitions)
            {
                if (definition.action.id == action)
                    return definition;
            }

            return std::nullopt;
        }

    private:
        std::span<const input_action_definition_t> _definitions{ };
    };
} // namespace carrot::input
