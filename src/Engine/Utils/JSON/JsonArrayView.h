//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "JsonAST.h"
#include "JsonValueView.h"

#include <cstdint>

namespace carrot::utils::json {
    class json_array_view_t
    {
    public:
        [[nodiscard]] uint32_t size() const;

        json_value_view_t operator[](uint32_t index) const;

        // Iteration
        class iterator
        {
        public:
            explicit iterator(const json_array_entry_t* e) : _entry(e) {}

            bool operator!=(const iterator& other) const { return _entry != other._entry; }
            void operator++() { _entry = _entry->next; }

            json_value_view_t operator*() const
            {
                return json_value_view_t{ _entry->value };
            }

        private:
            const json_array_entry_t* _entry;
        };

        [[nodiscard]] iterator begin() const;
        [[nodiscard]] iterator end() const;

    private:
        friend class json_value_view_t;
        explicit json_array_view_t(const json_value_t* v);

        const json_value_t* _value{ nullptr };
    };
} // namespace carrot::utils::json
