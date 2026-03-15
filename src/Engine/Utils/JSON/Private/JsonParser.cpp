//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "JsonParser.h"

namespace carrot::utils::json {
    // PUBLIC

    parser_t::parser_t(lexer_t& lex, core::memory::arena_t& arena) : _lexer{ lex }, _arena{ arena }
    {
        _current = _lexer.next_token();
    }

    json_value_t* parser_t::parse()
    {
        json_value_t* root{ parse_value() };

        // After parsing the root value, we should be at EOF
        if (_current.type != token_type::end_of_file)
        {
            LOG_CORE_FATAL("Did not reach end of file");
        }

        return root;
    }

    void parser_t::dump(json_value_t* v, const int indent/* = 0*/)
    {
        std::string pad{ std::string(indent * 4, ' ') };

        switch (v->type)
        {
            case json_type::object:
                LOG_CORE_INFO("{}object ({} entries)", pad, v->object.count);
                for (auto* e = v->object.head; e; e = e->next)
                {
                    LOG_CORE_INFO("{}  key: {}", pad, e->key);
                    dump(e->value, indent + 1);
                }
                break;

            case json_type::array:
                LOG_CORE_INFO("{}array ({} items)", pad, v->array.count);
                for (auto* e = v->array.head; e; e = e->next)
                    dump(e->value, indent + 1);
                break;

            case json_type::string:
                LOG_CORE_INFO("{}string \"{}\"", pad, v->string);
                break;

            case json_type::number:
                LOG_CORE_INFO("{}number {}", pad, v->number);
                break;

            case json_type::boolean:
                LOG_CORE_INFO("{}bool {}", pad, v->boolean);
                break;

            case json_type::null_value:
                LOG_CORE_INFO("{}null", pad);
                break;
        }
    }

    // PRIVATE

    json_value_t* parser_t::parse_value()
    {
        switch (_current.type)
        {
            case token_type::left_brace: return parse_object();
            case token_type::left_bracket: return parse_array();
            case token_type::string: return parse_string();
            case token_type::number: return parse_number();

            case token_type::true_literal:
            case token_type::false_literal:
            case token_type::null_literal:
                return parse_literal();

            default:
                LOG_CORE_FATAL("Unexpected token: {}", token_name(_current.type));
                return nullptr;
        }
    }

    json_value_t* parser_t::parse_object()
    {
        consume(token_type::left_brace, "Expected '{'");

        json_value_t* value{ _arena.emplace<json_value_t>() };
        value->type = json_type::object;
        value->object.head = nullptr;
        value->object.count = 0;

        json_object_entry_t** tail{ &value->object.head };

        if (match(token_type::right_brace)) return value;

        do
        {
            const token_t key{ consume(token_type::string, "Expected string key") };
            consume(token_type::colon, "Expected ':'");

            json_object_entry_t* entry{ _arena.emplace<json_object_entry_t>() };
            entry->key = std::string_view(_lexer.source().data() + key.text_offset, key.text_length);
            entry->value = parse_value();
            entry->next = nullptr;

            *tail = entry;
            tail = &entry->next;
            value->object.count++;
        } while (match(token_type::comma));

        consume(token_type::right_brace, "Expected '}'");
        return value;
    }

    json_value_t* parser_t::parse_array()
    {
        consume(token_type::left_bracket, "Expected '['");

        json_value_t* value{ _arena.emplace<json_value_t>() };
        value->type = json_type::array;
        value->array.head = nullptr;
        value->array.count = 0;

        json_array_entry_t** tail{ &value->array.head };

        if (match(token_type::right_bracket)) return value;

        do
        {
            json_array_entry_t* entry{ _arena.emplace<json_array_entry_t>() };
            entry->value = parse_value();
            entry->next = nullptr;

            *tail = entry;
            tail = &entry->next;
            value->array.count++;

        } while (match(token_type::comma));

        consume(token_type::right_bracket, "Expected ']'");
        return value;
    }

    json_value_t* parser_t::parse_string()
    {
        const token_t token{ consume(token_type::string, "Expected string") };

        json_value_t* value{ _arena.emplace<json_value_t>() };
        value->type = json_type::string;
        value->string = std::string_view(_lexer.source().data() + token.text_offset, token.text_length);

        return value;
    }

    json_value_t* parser_t::parse_number()
    {
        const token_t token{ consume(token_type::number, "Expected number") };

        json_value_t* value{ _arena.emplace<json_value_t>() };
        value->type = json_type::number;
        value->number = std::strtod(_lexer.source().data() + token.text_offset, nullptr);

        return value;
    }

    json_value_t* parser_t::parse_literal()
    {
        json_value_t* value{ _arena.emplace<json_value_t>() };

        if (match(token_type::true_literal))
        {
            value->type = json_type::boolean;
            value->boolean = true;
        }
        else if (match(token_type::false_literal))
        {
            value->type = json_type::boolean;
            value->boolean = false;
        }
        else
        {
            consume(token_type::null_literal, "Expected null");
            value->type = json_type::null_value;
        }

        return value;
    }

    token_t parser_t::consume(const token_type expected, const char* error_msg)
    {
        CE_ASSERT(_current.type == expected, "Unexpected token: {} ({})", token_name(_current.type), error_msg);

        const token_t token{ _current };
        _current = _lexer.next_token();

        return token;
    }

    bool parser_t::match(const token_type type)
    {
        if (_current.type != type) return false;

        _current = _lexer.next_token();
        return true;
    }
} // namespace carrot::utils::json
