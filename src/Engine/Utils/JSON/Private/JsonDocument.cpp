//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Utils/JSON/Public/JsonDocument.h"

#include "JsonParser.h"
#include "Utils/File/FileUtils.h"

namespace carrot::utils::json {
    namespace {
        [[nodiscard]] size_t recommended_arena_capacity(const size_t source_size) noexcept
        {
            constexpr size_t k_min_capacity{ 1024u * 1024u };
            constexpr size_t k_headroom_bytes{ 256u * 1024u };
            constexpr size_t k_source_multiplier{ 24u };

            const size_t scaled_capacity{ (source_size * k_source_multiplier) + k_headroom_bytes };
            return std::max(k_min_capacity, scaled_capacity);
        }
    } // namespace

    json_document_t::json_document_t()
    {
        _arena = new core::memory::arena_t(1024 * 1024);
    }

    json_document_t::~json_document_t()
    {
        delete _arena;
    }

    bool json_document_t::parse_from_file(const char* path)
    {
        reset();

        std::optional<std::string> file_data{ file::load_file_to_string(path) };
        if (!file_data)
        {
            _error_message = "Failed to load JSON file";
            return false;
        }

        _source = std::move(*file_data);
        return parse_from_memory(_source.data(), _source.size());
    }

    bool json_document_t::parse_from_memory(const char* data, [[maybe_unused]] size_t size)
    {
        const size_t required_capacity{ recommended_arena_capacity(size) };
        if (!_arena || _arena->capacity() < required_capacity)
        {
            delete _arena;
            _arena = new core::memory::arena_t(required_capacity);
        }

        lexer_t lexer(std::string_view{ data, size });
        parser_t parser(lexer, *_arena);

        _root = parser.parse();

        if (!_root)
        {
            _error_message = "JSON parse failed";
            return false;
        }

        return true;
    }

    void json_document_t::reset()
    {
        _arena->reset();
        _root = nullptr;
        _source.clear();
        _error_message = nullptr;
        _error_line = 0;
        _error_column = 0;
    }
} // namespace carrot::utils::json
