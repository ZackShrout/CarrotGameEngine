//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Memory/Arena.h"
#include "JsonAST.h"
#include "JsonLexer.h"

namespace carrot::utils::json {
    class parser_t
    {
    public:
        parser_t(lexer_t& lex, core::memory::arena_t& arena);

        json_value_t* parse();

        static void dump(json_value_t* v, int indent = 0);

    private:
        json_value_t* parse_value();
        json_value_t* parse_object();
        json_value_t* parse_array();

        json_value_t* parse_string();
        json_value_t* parse_number();
        json_value_t* parse_literal();

        token_t consume(token_type expected, const char* error_msg);
        bool match(token_type type);

        lexer_t& _lexer;
        core::memory::arena_t& _arena;
        token_t _current{ };
    };
} // namespace carrot::utils::json
