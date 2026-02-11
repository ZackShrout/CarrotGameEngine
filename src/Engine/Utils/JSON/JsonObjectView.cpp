//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "JsonObjectView.h"

#include "JsonArrayView.h"
#include "Common/CommonHeaders.h"

namespace carrot::utils::json {
    // PUBLIC

    bool json_object_view_t::has(const std::string_view key) const
    {
        for (const auto* e{ _value->object.head }; e; e = e->next)
        {
            if (e->key == key)
                return true;
        }

        return false;
    }

    json_value_view_t json_object_view_t::get(const std::string_view key) const
    {
        for (const auto* e{ _value->object.head }; e; e = e->next)
        {
            if (e->key == key)
                return json_value_view_t{ e->value };
        }

        return json_value_view_t{ nullptr };
    }

    uint32_t json_object_view_t::size() const
    {
        return _value->object.count;
    }

    json_object_view_t::iterator json_object_view_t::begin() const
    {
        return iterator{ _value->object.head };
    }

    json_object_view_t::iterator json_object_view_t::end() const
    {
        return iterator{ nullptr };
    }

    std::string_view json_object_view_t::get_string(std::string_view key) const
    {
        const json_value_view_t v{ get(key) };
        CE_ASSERT(v && v.is_string(), "Expected string for key '{}'", key);

        return v.as_string();
    }

    double json_object_view_t::get_number(std::string_view key) const
    {
        const json_value_view_t v{ get(key) };
        CE_ASSERT(v && v.is_number(), "Expected number for key '{}'", key);

        return v.as_number();
    }

    bool json_object_view_t::get_bool(std::string_view key) const
    {
        const json_value_view_t v{ get(key) };
        CE_ASSERT(v && v.is_bool(), "Expected boolean for key '{}'", key);

        return v.as_bool();
    }

    std::string_view json_object_view_t::get_string_or(std::string_view key, std::string_view fallback) const
    {
        const json_value_view_t v{ get(key) };

        return v.as_string_or(fallback);
    }

    double json_object_view_t::get_number_or(std::string_view key, double fallback) const
    {
        const json_value_view_t v{ get(key) };

        return v.as_number_or(fallback);
    }

    bool json_object_view_t::get_bool_or(std::string_view key, bool fallback) const
    {
        const json_value_view_t v{ get(key) };

        return v.as_bool_or(fallback);
    }

    json_object_view_t json_object_view_t::get_object(std::string_view key) const
    {
        const json_value_view_t v{ get(key) };
        CE_ASSERT(v && v.is_object(), "Expected object for key '{}'", key);

        return v.as_object();
    }

    json_array_view_t json_object_view_t::get_array(std::string_view key) const
    {
        const json_value_view_t v{ get(key) };
        CE_ASSERT(v && v.is_array(), "Expected array for key '{}'", key);

        return v.as_array();
    }

    // PRIVATE

    json_object_view_t::json_object_view_t(const json_value_t* v) : _value{ v }
    {
        CE_ASSERT(v && v->type == json_type::object, "JSON value is not an object");
    }
} // namespace carrot::utils::json
