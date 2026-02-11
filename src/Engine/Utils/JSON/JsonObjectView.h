//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "JsonAST.h"
#include "JsonValueView.h"

#include <string_view>

namespace carrot::utils::json {
    class json_object_view_t
    {
    public:
        [[nodiscard]] bool has(std::string_view key) const;
        [[nodiscard]] json_value_view_t get(std::string_view key) const;
        [[nodiscard]] uint32_t size() const;

        // Iteration
        class iterator
        {
        public:
            explicit iterator(const json_object_entry_t* e) : _entry(e) {}

            bool operator!=(const iterator& other) const { return _entry != other._entry; }
            void operator++() { _entry = _entry->next; }

            std::pair<std::string_view, json_value_view_t> operator*() const
            {
                return { _entry->key, json_value_view_t{ _entry->value } };
            }

        private:
            const json_object_entry_t* _entry;
        };

        [[nodiscard]] iterator begin() const;
        [[nodiscard]] iterator end() const;

        [[nodiscard]] std::string_view get_string(std::string_view key) const;
        [[nodiscard]] double get_number(std::string_view key) const;
        [[nodiscard]] bool get_bool(std::string_view key) const;
        [[nodiscard]] std::string_view get_string_or(std::string_view key, std::string_view fallback) const;
        [[nodiscard]] double get_number_or(std::string_view key, double fallback) const;
        [[nodiscard]] bool get_bool_or(std::string_view key, bool fallback) const;
        [[nodiscard]] json_object_view_t get_object(std::string_view key) const;
        [[nodiscard]] json_array_view_t get_array(std::string_view key) const;

    private:
        friend class json_value_view_t;
        explicit json_object_view_t(const json_value_t* v);

        const json_value_t* _value{ nullptr };
    };
} // namespace carrot::utils::json
