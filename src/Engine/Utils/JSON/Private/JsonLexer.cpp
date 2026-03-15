//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "JsonLexer.h"

#include <cctype>

namespace carrot::utils::json {
    token_t lexer_t::next_token()
    {
        skip_whitespace();

        const char c{ advance() };

        if (c == '\0') return make_token(token_type::end_of_file);

        switch (c)
        {
            case '{': return make_token(token_type::left_brace);
            case '}': return make_token(token_type::right_brace);
            case '[': return make_token(token_type::left_bracket);
            case ']': return make_token(token_type::right_bracket);
            case ',': return make_token(token_type::comma);
            case ':': return make_token(token_type::colon);
            case '"': return lex_string();
            default:
                if (std::isdigit(c) || c == '-')
                {
                    _cursor--; // unread
                    _column--;

                    return lex_number();
                }

                if (std::isalpha(c))
                {
                    _cursor--; // unread
                    _column--;

                    return lex_keyword();
                }

                return make_error_token();
        }
    }

    char lexer_t::peek() const
    {
        if (_cursor >= _source.size()) return '\0';

        return _source[_cursor];
    }

    char lexer_t::advance()
    {
        const char c{ peek() };

        if (c == '\0') return c;

        ++_cursor;

        if (c == '\n')
        {
            ++_line;
            _column = 1;
        }
        else
        {
            ++_column;
        }

        return c;
    }

    bool lexer_t::match(const char expected)
    {
        if (peek() != expected) return false;

        advance();

        return true;
    }

    void lexer_t::skip_whitespace()
    {
        for (;;)
        {
            const char c{ peek() };
            switch (c)
            {
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                    advance();
                    break;
                default: return;
            }
        }
    }

    token_t lexer_t::make_token(const token_type type) const
    {
        token_t token{};
        token.type = type;
        token.line = _line;
        token.column = _column;
        token.text_offset = 0;
        token.text_length = 0;

        return token;
    }

    token_t lexer_t::make_error_token() const
    {
        token_t token{};
        token.type = token_type::invalid;
        token.line = _line;
        token.column = _column;
        token.text_offset = 0;
        token.text_length = 0;

        return token;
    }

    token_t lexer_t::lex_string()
    {
        // NOTE: The opening quote will already be consumed
        const uint32_t start_line{ _line };
        const uint32_t start_column{ _column };
        const size_t start{ _cursor };

        for (;;)
        {
            const char c{ advance() };
            if (c == '\0') return make_error_token();
            if (c == '"') break;
            if (c == '\\') advance(); // skip escape character
        }

        const size_t end{ _cursor - 1 };

        token_t token{};
        token.type = token_type::string;
        token.line = start_line;
        token.column = start_column;
        token.text_offset = static_cast<uint32_t>(start);
        token.text_length = static_cast<uint32_t>(end - start);

        return token;
    }

    token_t lexer_t::lex_number()
    {
        const uint32_t start_line{ _line };
        const uint32_t start_column{ _column };
        const size_t start{ _cursor };

        if (peek() == '-') advance();

        while (std::isdigit(peek())) advance();

        if (peek() == '.')
        {
            advance();

            while (std::isdigit(peek())) advance();
        }

        if (peek() == 'e' || peek() == 'E')
        {
            advance();

            if (peek() == '+' || peek() == '-') advance();

            while (std::isdigit(peek())) advance();
        }

        const size_t end{ _cursor };

        token_t token{};
        token.type = token_type::number;
        token.line = start_line;
        token.column = start_column;
        token.text_offset = static_cast<uint32_t>(start);
        token.text_length = static_cast<uint32_t>(end - start);

        return token;
    }

    token_t lexer_t::lex_keyword()
    {
        const uint32_t start_line{ _line };
        const uint32_t start_column{ _column };
        const size_t start{ _cursor };

        while (std::isalpha(peek())) advance();

        const size_t length{ _cursor - start };
        const std::string_view text{ _source.substr(start, length) };

        if (text == "true") return { token_type::true_literal, start_line, start_column, 0, 0 };
        if (text == "false") return { token_type::false_literal, start_line, start_column, 0, 0 };
        if (text == "null") return { token_type::null_literal, start_line, start_column, 0, 0 };

        return make_error_token();
    }
} // namespace carrot::utils::json
