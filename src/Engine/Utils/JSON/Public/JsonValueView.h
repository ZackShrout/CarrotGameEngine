//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string_view>

namespace carrot::utils::json {
    struct json_value_t;
    class json_object_view_t;
    class json_array_view_t;

    class json_value_view_t
    {
    public:
        [[nodiscard]] bool is_object() const;
        [[nodiscard]] bool is_array() const;
        [[nodiscard]] bool is_string() const;
        [[nodiscard]] bool is_number() const;
        [[nodiscard]] bool is_bool() const;
        [[nodiscard]] bool is_null() const;

        [[nodiscard]] std::string_view as_string_or(std::string_view fallback) const;
        [[nodiscard]] double as_number_or(double fallback) const;
        [[nodiscard]] bool as_bool_or(bool fallback) const;

        [[nodiscard]] std::string_view as_string() const;
        [[nodiscard]] double as_number() const;
        [[nodiscard]] bool as_bool() const;

        [[nodiscard]] json_object_view_t as_object() const;
        [[nodiscard]] json_array_view_t as_array() const;

        explicit operator bool() const { return _value != nullptr; }

    private:
        friend class json_document_t;
        friend class json_object_view_t;
        friend class json_array_view_t;

        explicit json_value_view_t(const json_value_t* v) : _value(v) {}

        const json_value_t* _value{ nullptr };
    };
} // namespace carrot::utils::json
