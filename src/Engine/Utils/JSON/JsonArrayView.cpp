//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "JsonArrayView.h"

#include "Common/CommonHeaders.h"

namespace carrot::utils::json {
    // PUBLIC

    uint32_t json_array_view_t::size() const
    {
        return _value->array.count;
    }

    json_value_view_t json_array_view_t::operator[](const uint32_t index) const
    {
        CE_ASSERT(index < _value->array.count, "JSON array index out of bounds");

        uint32_t i = 0;
        for (auto* e = _value->array.head; e; e = e->next)
        {
            if (i == index)
                return json_value_view_t{ e->value };
            ++i;
        }

        return json_value_view_t{ nullptr }; // unreachable if count is correct
    }

    json_array_view_t::iterator json_array_view_t::begin() const
    {
        return iterator{ _value->array.head };
    }

    json_array_view_t::iterator json_array_view_t::end() const
    {
        return iterator{ nullptr };
    }

    // PRIVATE

    json_array_view_t::json_array_view_t(const json_value_t* v) : _value{ v }
    {
        CE_ASSERT(v && v->type == json_type::array, "JSON value is not an array");
    }
} // namespace carrot::utils::json
