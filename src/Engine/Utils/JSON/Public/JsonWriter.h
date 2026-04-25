//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::utils::json {
    enum class json_writer_style_t : std::uint8_t
    {
        compact = 0,
        pretty
    };

    class json_writer_t
    {
    public:
        explicit json_writer_t(json_writer_style_t style = json_writer_style_t::pretty) noexcept;

        void reset() noexcept;

        void begin_object();
        void end_object();
        void begin_array();
        void end_array();

        void key(std::string_view name);

        void value(std::string_view value);
        void value(const char* value);
        void value(double value);
        void value(std::uint64_t value);
        void value(std::uint32_t value);
        void value(std::int32_t value);
        void value(bool value);
        void null();

        [[nodiscard]] const std::string& str() const noexcept { return _output; }
        [[nodiscard]] std::string take();

    private:
        enum class container_kind_t : std::uint8_t
        {
            object = 0,
            array
        };

        struct container_state_t
        {
            container_kind_t kind{ container_kind_t::object };
            bool has_elements{ false };
            bool expecting_key{ true };
        };

        void before_value();
        void before_key();
        void after_value();
        void write_escaped_string(std::string_view value);
        void write_indent();
        [[nodiscard]] bool pretty() const noexcept { return _style == json_writer_style_t::pretty; }

        json_writer_style_t _style;
        std::string _output;
        std::vector<container_state_t> _stack;
    };

    class json_object_scope_t
    {
    public:
        explicit json_object_scope_t(json_writer_t& writer) noexcept : _writer(writer)
        {
            _writer.begin_object();
        }

        ~json_object_scope_t()
        {
            _writer.end_object();
        }

        json_object_scope_t(const json_object_scope_t&) = delete;
        json_object_scope_t& operator=(const json_object_scope_t&) = delete;

    private:
        json_writer_t& _writer;
    };

    class json_array_scope_t
    {
    public:
        explicit json_array_scope_t(json_writer_t& writer) noexcept : _writer(writer)
        {
            _writer.begin_array();
        }

        ~json_array_scope_t()
        {
            _writer.end_array();
        }

        json_array_scope_t(const json_array_scope_t&) = delete;
        json_array_scope_t& operator=(const json_array_scope_t&) = delete;

    private:
        json_writer_t& _writer;
    };
} // namespace carrot::utils::json
