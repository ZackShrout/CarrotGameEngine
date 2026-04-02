//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "JsonValueView.h"
#include "Utils/Assert.h"

#include <span>
#include <string_view>

namespace carrot::utils::json {
    struct json_object_entry_t;
    struct json_value_t;

    template<typename Enum>
    struct json_enum_entry_t
    {
        std::string_view name{ };
        Enum value;
    };

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
            void operator++();
            std::pair<std::string_view, json_value_view_t> operator*() const;

        private:
            const json_object_entry_t* _entry;
        };

        [[nodiscard]] iterator begin() const;
        [[nodiscard]] iterator end() const;

        [[nodiscard]] json_value_view_t require(std::string_view key) const;
        [[nodiscard]] json_object_view_t require_object(std::string_view key) const;
        [[nodiscard]] json_array_view_t require_array(std::string_view key) const;

        [[nodiscard]] std::string_view get_string(std::string_view key) const;
        [[nodiscard]] double get_number(std::string_view key) const;
        [[nodiscard]] bool get_bool(std::string_view key) const;
        [[nodiscard]] std::string_view get_string_or(std::string_view key, std::string_view fallback) const;
        [[nodiscard]] double get_number_or(std::string_view key, double fallback) const;
        [[nodiscard]] bool get_bool_or(std::string_view key, bool fallback) const;
        [[nodiscard]] json_object_view_t get_object(std::string_view key) const;
        [[nodiscard]] json_array_view_t get_array(std::string_view key) const;

        template<typename Enum>
        Enum get_enum(std::string_view key, std::span<const json_enum_entry_t<Enum>> map, Enum fallback) const;

        template<typename Enum>
        Enum require_enum(std::string_view key, std::span<const json_enum_entry_t<Enum>> map) const;

    private:
        friend class json_value_view_t;
        explicit json_object_view_t(const json_value_t* v);

        const json_value_t* _value{ nullptr };
    };

    template<typename Enum>
    Enum json_object_view_t::get_enum(std::string_view key, std::span<const json_enum_entry_t<Enum>> map,
        Enum fallback) const
    {
        const json_value_view_t v{ get(key) };

        if (!v)
            return fallback;

        CE_ASSERT(v.is_string(), "Key '{}' must be a string", key);

        const std::string_view s{ v.as_string() };

        // Convention: "default" always maps to fallback
        if (s == "default")
            return fallback;

        for (const auto& e : map)
        {
            if (e.name == s)
                return e.value;
        }

        CE_ASSERT(false, "Invalid enum value '{}' for key '{}'", s, key);

        return fallback;
    }

    template<typename Enum>
    Enum json_object_view_t::require_enum(std::string_view key, std::span<const json_enum_entry_t<Enum>> map) const
    {
        const json_value_view_t v{ require(key) };
        CE_ASSERT(v.is_string(), "Key '{}' must be a string", key);

        const std::string_view s = v.as_string();

        for (const auto& e : map)
        {
            if (e.name == s)
                return e.value;
        }

        CE_ASSERT(false, "Invalid enum value '{}' for key '{}'", s, key);

        return map[0].value;
    }
} // namespace carrot::utils::json
