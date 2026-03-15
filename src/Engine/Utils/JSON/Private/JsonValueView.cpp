//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "../Public/JsonValueView.h"

#include "JsonAST.h"
#include "Utils/JSON/Public/JsonObjectView.h"
#include "Utils/JSON/Public/JsonArrayView.h"
#include "../../../Core/CoreDefines.h"

namespace carrot::utils::json {
    bool json_value_view_t::is_object() const
    {
        return _value && _value->type == json_type::object;
    }

    bool json_value_view_t::is_array() const
    {
        return _value && _value->type == json_type::array;
    }

    bool json_value_view_t::is_string() const
    {
        return _value && _value->type == json_type::string;
    }

    bool json_value_view_t::is_number() const
    {
        return _value && _value->type == json_type::number;
    }

    bool json_value_view_t::is_bool() const
    {
        return _value && _value->type == json_type::boolean;
    }

    bool json_value_view_t::is_null() const
    {
        return !_value || _value->type == json_type::null_value;
    }

    std::string_view json_value_view_t::as_string_or(const std::string_view fallback) const
    {
        return is_string() ? _value->string : fallback;
    }

    double json_value_view_t::as_number_or(const double fallback) const
    {
        return is_number() ? _value->number : fallback;
    }

    bool json_value_view_t::as_bool_or(const bool fallback) const
    {
        return is_bool() ? _value->boolean : fallback;
    }

    std::string_view json_value_view_t::as_string() const
    {
        CE_ASSERT(is_string(), "JSON value is not a string");

        return _value->string;
    }

    double json_value_view_t::as_number() const
    {
        CE_ASSERT(is_number(), "JSON value is not a number");

        return _value->number;
    }

    bool json_value_view_t::as_bool() const
    {
        CE_ASSERT(is_bool(), "JSON value is not a boolean");

        return _value->boolean;
    }

    json_object_view_t json_value_view_t::as_object() const
    {
        CE_ASSERT(is_object(), "JSON value is not an object");

        return json_object_view_t{ _value };
    }

    json_array_view_t json_value_view_t::as_array() const
    {
        CE_ASSERT(is_array(), "JSON value is not an array");

        return json_array_view_t{ _value };
    }
} // namespace carrot::utils::json
