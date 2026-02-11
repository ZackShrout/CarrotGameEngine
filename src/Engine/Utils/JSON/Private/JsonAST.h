//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string_view>

namespace carrot::utils::json {
    struct json_value_t;

    struct json_object_entry_t
    {
        std::string_view        key;
        json_value_t*           value;
        json_object_entry_t*    next;
    };

    struct json_array_entry_t
    {
        json_value_t*           value;
        json_array_entry_t*     next;
    };

    enum class json_type : uint8_t
    {
        object,
        array,
        string,
        number,
        boolean,
        null_value
    };

    struct json_value_t
    {
        json_value_t() {}

        json_type type{ json_type::null_value };

        union
        {
            // object
            struct
            {
                json_object_entry_t*    head{ nullptr };
                uint32_t                count{ 0 };
            } object{ };

            // array
            struct
            {
                json_array_entry_t*     head{ nullptr };
                uint32_t                count{ 0 };
            } array;

            // primitives
            std::string_view            string;
            double                      number;
            bool                        boolean;
        };
    };
} // namespace carrot::utils::json
