//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Utils/JSON/Public/JsonDocument.h"

#include "JsonParser.h"
#include "Utils/File/FileUtils.h"

#include <cstdio>
#include <cstdlib>

namespace carrot::utils::json {
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
        lexer_t lexer(data);
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
