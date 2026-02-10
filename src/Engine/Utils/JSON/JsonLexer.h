//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#pragma once

#include "JsonToken.h"
#include <string_view>

namespace carrot::utils::json {
    class lexer_t
    {
    public:
        explicit lexer_t(const std::string_view source) : _source{ source } {}

        token_t next_token();

        [[nodiscard]] std::string_view source() const noexcept { return _source; }

    private:
        char peek() const;
        char advance();
        bool match(char expected);

        void skip_whitespace();

        token_t make_token(token_type type) const;
        token_t make_error_token() const;

        token_t lex_string();
        token_t lex_number();
        token_t lex_keyword();

        std::string_view _source;
        size_t _cursor{ 0 };
        uint32_t _line{ 1 };
        uint32_t _column{ 1 };
    };
} // namespace carrot::utils::json
