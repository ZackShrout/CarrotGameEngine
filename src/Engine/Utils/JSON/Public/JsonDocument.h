//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "JsonValueView.h"
#include "JsonObjectView.h"
#include "JsonArrayView.h"
#include "Common/CommonHeaders.h"
#include "Core/Memory/Arena.h"

namespace carrot::utils::json {
    struct json_value_t;

    class json_document_t
    {
    public:
        json_document_t();
        ~json_document_t();

        DISABLE_COPY(json_document_t)

        json_document_t(json_document_t&& other) noexcept { *this = std::move(other); }
        json_document_t& operator=(json_document_t&& other) noexcept
        {
            if (this == &other)
                return *this;

            delete _arena;

            _arena = other._arena;
            _root = other._root;

            _error_message = other._error_message;
            _error_line = other._error_line;
            _error_column = other._error_column;

            other._arena = nullptr;
            other._root = nullptr;

            return *this;
        }

        // Parse entry points
        bool parse_from_file(const char* path);
        bool parse_from_memory(const char* data, size_t size);

        void reset();

        // Root access
        [[nodiscard]] json_value_view_t root() const { return json_value_view_t{ _root }; }

        [[nodiscard]] bool valid() const { return _root != nullptr; }

        // Diagnostics
        [[nodiscard]] const char* error_message() const { return _error_message; }
        [[nodiscard]] int error_line() const { return _error_line; }
        [[nodiscard]] int error_column() const { return _error_column; }

    private:
        core::memory::arena_t* _arena{ nullptr };
        json_value_t* _root{ nullptr };

        std::string _source;
        const char* _error_message{ nullptr };
        int _error_line{ 0 };
        int _error_column{ 0 };
    };
} // namespace carrot::utils::json
