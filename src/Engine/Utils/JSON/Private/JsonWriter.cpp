//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Utils/JSON/Public/JsonWriter.h"

#include <charconv>

namespace carrot::utils::json {
    json_writer_t::json_writer_t(const json_writer_style_t style) noexcept
        : _style(style)
    {
    }

    void json_writer_t::reset() noexcept
    {
        _output.clear();
        _stack.clear();
    }

    void json_writer_t::begin_object()
    {
        before_value();
        _output.push_back('{');
        _stack.push_back(container_state_t{
            .kind = container_kind_t::object,
            .has_elements = false,
            .expecting_key = true
        });
    }

    void json_writer_t::end_object()
    {
        CE_ASSERT(!_stack.empty() && _stack.back().kind == container_kind_t::object,
                  "JSON writer stack mismatch on end_object()");
        const container_state_t state{ _stack.back() };
        _stack.pop_back();

        if (pretty() && state.has_elements)
        {
            _output.push_back('\n');
            write_indent();
        }

        _output.push_back('}');
        after_value();
    }

    void json_writer_t::begin_array()
    {
        before_value();
        _output.push_back('[');
        _stack.push_back(container_state_t{
            .kind = container_kind_t::array,
            .has_elements = false,
            .expecting_key = false
        });
    }

    void json_writer_t::end_array()
    {
        CE_ASSERT(!_stack.empty() && _stack.back().kind == container_kind_t::array,
                  "JSON writer stack mismatch on end_array()");
        const container_state_t state{ _stack.back() };
        _stack.pop_back();

        if (pretty() && state.has_elements)
        {
            _output.push_back('\n');
            write_indent();
        }

        _output.push_back(']');
        after_value();
    }

    void json_writer_t::key(const std::string_view name)
    {
        before_key();
        write_escaped_string(name);
        _output += pretty() ? ": " : ":";

        CE_ASSERT(!_stack.empty() && _stack.back().kind == container_kind_t::object,
                  "JSON writer key() requires object scope");
        _stack.back().expecting_key = false;
    }

    void json_writer_t::value(const std::string_view value)
    {
        before_value();
        write_escaped_string(value);
        after_value();
    }

    void json_writer_t::value(const char* const value)
    {
        if (!value)
        {
            null();
            return;
        }

        this->value(std::string_view{ value });
    }

    void json_writer_t::value(const double number)
    {
        before_value();

        char buffer[64]{ };
        const auto [ptr, ec]{ std::to_chars(buffer, buffer + sizeof(buffer), number) };
        CE_ASSERT(ec == std::errc(), "Failed to format JSON floating-point value");
        _output.append(buffer, ptr);

        after_value();
    }

    void json_writer_t::value(const std::uint64_t value)
    {
        before_value();
        _output += std::to_string(value);
        after_value();
    }

    void json_writer_t::value(const std::uint32_t number)
    {
        this->value(static_cast<std::uint64_t>(number));
    }

    void json_writer_t::value(const std::int32_t value)
    {
        before_value();
        _output += std::to_string(value);
        after_value();
    }

    void json_writer_t::value(const bool value)
    {
        before_value();
        _output += value ? "true" : "false";
        after_value();
    }

    void json_writer_t::null()
    {
        before_value();
        _output += "null";
        after_value();
    }

    std::string json_writer_t::take()
    {
        std::string result{ std::move(_output) };
        reset();
        return result;
    }

    void json_writer_t::before_value()
    {
        if (_stack.empty())
            return;

        container_state_t& parent{ _stack.back() };
        if (parent.kind == container_kind_t::array)
        {
            if (parent.has_elements)
                _output.push_back(',');

            if (pretty())
            {
                _output.push_back('\n');
                write_indent();
            }
            return;
        }

        CE_ASSERT(!parent.expecting_key, "JSON object value emitted before key()");
    }

    void json_writer_t::before_key()
    {
        CE_ASSERT(!_stack.empty() && _stack.back().kind == container_kind_t::object,
                  "JSON writer key() requires object scope");

        container_state_t& parent{ _stack.back() };
        CE_ASSERT(parent.expecting_key, "JSON object expected a value before the next key()");

        if (parent.has_elements)
            _output.push_back(',');

        if (pretty())
        {
            _output.push_back('\n');
            write_indent();
        }
    }

    void json_writer_t::after_value()
    {
        if (_stack.empty())
            return;

        container_state_t& parent{ _stack.back() };
        parent.has_elements = true;
        if (parent.kind == container_kind_t::object)
            parent.expecting_key = true;
    }

    void json_writer_t::write_escaped_string(const std::string_view value)
    {
        _output.push_back('"');
        for (const char ch : value)
        {
            switch (ch)
            {
                case '\\': _output += "\\\\"; break;
                case '"': _output += "\\\""; break;
                case '\n': _output += "\\n"; break;
                case '\r': _output += "\\r"; break;
                case '\t': _output += "\\t"; break;
                default: _output.push_back(ch); break;
            }
        }
        _output.push_back('"');
    }

    void json_writer_t::write_indent()
    {
        if (!pretty())
            return;

        _output.append(_stack.size() * 2u, ' ');
    }
} // namespace carrot::utils::json
