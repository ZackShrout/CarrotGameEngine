//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#pragma once

#include <cstdint>

namespace carrot::utils::json
{
    enum class token_type : uint8_t
    {
        invalid,
        end_of_file,

        // Structural
        left_brace,    // {
        right_brace,   // }
        left_bracket,  // [
        right_bracket, // ]
        comma,         // ,
        colon,         // :

        // Literals
        string,
        number,
        true_literal,
        false_literal,
        null_literal
    };

    struct token_t
    {
        token_type type;

        uint32_t line;
        uint32_t column;

        // For string / number tokens
        uint32_t text_offset;
        uint32_t text_length;
    };

    static const char* token_name(const token_type t)
    {
        switch (t)
        {
            case token_type::left_brace: return "{";
            case token_type::right_brace: return "}";
            case token_type::left_bracket: return "[";
            case token_type::right_bracket: return "]";
            case token_type::string: return "string";
            case token_type::number: return "number";
            case token_type::true_literal: return "true";
            case token_type::false_literal: return "false";
            case token_type::null_literal: return "null";
            case token_type::comma: return ",";
            case token_type::colon: return ":";
            case token_type::end_of_file: return "EOF";
            default: return "INVALID";
        }
    }
} // namespace carrot::utils::json
