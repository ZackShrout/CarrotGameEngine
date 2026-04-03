//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ActionMap.h"

#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::input {
    namespace {
        [[nodiscard]] std::string normalize_name(std::string_view value)
        {
            std::string normalized;
            normalized.reserve(value.size());

            for (const char ch : value)
            {
                if (ch == ' ' || ch == '_' || ch == '-')
                    continue;

                normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }

            return normalized;
        }

        [[nodiscard]] key_code key_code_from_string(const std::string_view raw_value) noexcept
        {
            const std::string value{ normalize_name(raw_value) };

            if (value == "a") return key_code::a;
            if (value == "b") return key_code::b;
            if (value == "c") return key_code::c;
            if (value == "d") return key_code::d;
            if (value == "e") return key_code::e;
            if (value == "f") return key_code::f;
            if (value == "g") return key_code::g;
            if (value == "h") return key_code::h;
            if (value == "i") return key_code::i;
            if (value == "j") return key_code::j;
            if (value == "k") return key_code::k;
            if (value == "l") return key_code::l;
            if (value == "m") return key_code::m;
            if (value == "n") return key_code::n;
            if (value == "o") return key_code::o;
            if (value == "p") return key_code::p;
            if (value == "q") return key_code::q;
            if (value == "r") return key_code::r;
            if (value == "s") return key_code::s;
            if (value == "t") return key_code::t;
            if (value == "u") return key_code::u;
            if (value == "v") return key_code::v;
            if (value == "w") return key_code::w;
            if (value == "x") return key_code::x;
            if (value == "y") return key_code::y;
            if (value == "z") return key_code::z;

            if (value == "0" || value == "digit0") return key_code::digit0;
            if (value == "1" || value == "digit1") return key_code::digit1;
            if (value == "2" || value == "digit2") return key_code::digit2;
            if (value == "3" || value == "digit3") return key_code::digit3;
            if (value == "4" || value == "digit4") return key_code::digit4;
            if (value == "5" || value == "digit5") return key_code::digit5;
            if (value == "6" || value == "digit6") return key_code::digit6;
            if (value == "7" || value == "digit7") return key_code::digit7;
            if (value == "8" || value == "digit8") return key_code::digit8;
            if (value == "9" || value == "digit9") return key_code::digit9;

            if (value == "f1") return key_code::f1;
            if (value == "f2") return key_code::f2;
            if (value == "f3") return key_code::f3;
            if (value == "f4") return key_code::f4;
            if (value == "f5") return key_code::f5;
            if (value == "f6") return key_code::f6;
            if (value == "f7") return key_code::f7;
            if (value == "f8") return key_code::f8;
            if (value == "f9") return key_code::f9;
            if (value == "f10") return key_code::f10;
            if (value == "f11") return key_code::f11;
            if (value == "f12") return key_code::f12;

            if (value == "escape" || value == "esc") return key_code::escape;
            if (value == "enter" || value == "return") return key_code::enter;
            if (value == "tab") return key_code::tab;
            if (value == "backspace") return key_code::backspace;
            if (value == "space" || value == "spacebar") return key_code::space;
            if (value == "delete" || value == "del") return key_code::del;
            if (value == "insert" || value == "ins") return key_code::insert;

            if (value == "left" || value == "leftarrow") return key_code::left;
            if (value == "right" || value == "rightarrow") return key_code::right;
            if (value == "up" || value == "uparrow") return key_code::up;
            if (value == "down" || value == "downarrow") return key_code::down;

            if (value == "leftshift") return key_code::left_shift;
            if (value == "rightshift") return key_code::right_shift;
            if (value == "leftcontrol" || value == "leftctrl") return key_code::left_control;
            if (value == "rightcontrol" || value == "rightctrl") return key_code::right_control;
            if (value == "leftalt") return key_code::left_alt;
            if (value == "rightalt") return key_code::right_alt;
            if (value == "leftsuper" || value == "leftcmd" || value == "leftwin") return key_code::left_super;
            if (value == "rightsuper" || value == "rightcmd" || value == "rightwin") return key_code::right_super;

            return key_code::unknown;
        }

        [[nodiscard]] std::optional<uint8_t> modifiers_from_json(const utils::json::json_value_view_t value) noexcept
        {
            if (!value)
                return uint8_t{ 0 };

            if (value.is_string())
            {
                const std::string modifier_name{ normalize_name(value.as_string()) };
                if (modifier_name == "shift") return static_cast<uint8_t>(modifier::shift);
                if (modifier_name == "control" || modifier_name == "ctrl") return static_cast<uint8_t>(modifier::control);
                if (modifier_name == "alt" || modifier_name == "option") return static_cast<uint8_t>(modifier::alt);
                if (modifier_name == "super" || modifier_name == "cmd" || modifier_name == "win") return static_cast<uint8_t>(modifier::super);
                return std::nullopt;
            }

            if (!value.is_array())
                return std::nullopt;

            uint8_t mods{ 0 };
            for (const utils::json::json_value_view_t modifier_value : value.as_array())
            {
                if (!modifier_value.is_string())
                    return std::nullopt;

                const std::optional<uint8_t> parsed{ modifiers_from_json(modifier_value) };
                if (!parsed.has_value())
                    return std::nullopt;

                mods |= *parsed;
            }

            return mods;
        }
    } // namespace

    void input_action_map_t::bind(std::string action, const key_code key, const uint8_t required_mods)
    {
        if (action.empty() || key == key_code::unknown)
            return;

        _bindings.emplace_back(action_binding_t{
            .action = std::move(action),
            .key = key,
            .required_mods = required_mods
        });
    }

    void input_action_map_t::clear() noexcept
    {
        _bindings.clear();
        _pressed_actions.clear();
    }

    bool input_action_map_t::load_bindings_from_memory(const char* data, const size_t size)
    {
        if (!data || size == 0)
        {
            LOG_CORE_WARN("Input binding config is empty");
            return false;
        }

        utils::json::json_document_t doc;
        if (!doc.parse_from_memory(data, size))
        {
            LOG_CORE_WARN("Failed to parse input binding config: {} (line {}, column {})",
                          doc.error_message() ? doc.error_message() : "unknown parse error",
                          doc.error_line(),
                          doc.error_column());
            return false;
        }

        if (!doc.root().is_object())
        {
            LOG_CORE_WARN("Input binding config root must be an object");
            return false;
        }

        const utils::json::json_object_view_t root{ doc.root().as_object() };
        if (!root.has("bindings"))
        {
            LOG_CORE_WARN("Input binding config is missing required 'bindings' array");
            return false;
        }

        const utils::json::json_value_view_t bindings_value{ root.get("bindings") };
        if (!bindings_value.is_array())
        {
            LOG_CORE_WARN("Input binding config field 'bindings' must be an array");
            return false;
        }

        std::vector<action_binding_t> parsed_bindings;
        for (const utils::json::json_value_view_t binding_value : bindings_value.as_array())
        {
            if (!binding_value.is_object())
            {
                LOG_CORE_WARN("Input binding config contains a non-object binding entry");
                return false;
            }

            const utils::json::json_object_view_t binding{ binding_value.as_object() };
            const std::string_view action{ binding.get_string("action") };
            const std::string_view key_name{ binding.get_string("key") };

            if (!action.data() || action.empty())
            {
                LOG_CORE_WARN("Input binding config contains a binding with a missing/empty 'action'");
                return false;
            }

            if (!key_name.data() || key_name.empty())
            {
                LOG_CORE_WARN("Input binding config action '{}' is missing a valid 'key'", action);
                return false;
            }

            const key_code key{ key_code_from_string(key_name) };
            if (key == key_code::unknown)
            {
                LOG_CORE_WARN("Input binding config action '{}' uses unknown key '{}'", action, key_name);
                return false;
            }

            const std::optional<uint8_t> required_mods{
                modifiers_from_json(binding.get("mods"))
            };
            if (!required_mods.has_value())
            {
                LOG_CORE_WARN("Input binding config action '{}' has invalid modifier data", action);
                return false;
            }

            parsed_bindings.emplace_back(action_binding_t{
                .action = std::string{ action },
                .key = key,
                .required_mods = *required_mods
            });
        }

        clear();
        _bindings = std::move(parsed_bindings);
        return true;
    }

    bool input_action_map_t::load_bindings_from_file(const io::virtual_file_system_t& vfs,
                                                     const std::string_view virtual_path)
    {
        const std::optional<std::filesystem::path> native_path{ vfs.resolve_native_path(virtual_path) };
        if (!native_path.has_value())
        {
            LOG_CORE_WARN("Input binding config '{}' could not be resolved", virtual_path);
            return false;
        }

        const std::optional<std::string> file_contents{ utils::file::load_file_to_string(*native_path) };
        if (!file_contents.has_value())
        {
            LOG_CORE_WARN("Input binding config '{}' could not be loaded from '{}'",
                          virtual_path,
                          utils::file::to_log_string(*native_path));
            return false;
        }

        return load_bindings_from_memory(file_contents->data(), file_contents->size());
    }

    void input_action_map_t::handle_key_event(const events::key_event_t& e) noexcept
    {
        if (e._action != events::key_action::press && e._action != events::key_action::release)
            return;

        for (const action_binding_t& binding : _bindings)
        {
            if (!binding_matches_event(binding, e))
                continue;

            _pressed_actions[binding.action] = (e._action == events::key_action::press);
        }
    }

    bool input_action_map_t::matches(const std::string_view action, const events::key_event_t& e) const noexcept
    {
        for (const action_binding_t& binding : _bindings)
        {
            if (binding.action != action)
                continue;

            if (binding_matches_event(binding, e))
                return true;
        }

        return false;
    }

    bool input_action_map_t::is_pressed(const std::string_view action) const noexcept
    {
        const auto it{ _pressed_actions.find(std::string{ action }) };
        return it != _pressed_actions.end() && it->second;
    }

    bool input_action_map_t::binding_matches_event(const action_binding_t& binding,
                                                   const events::key_event_t& e) const noexcept
    {
        if (binding.key != e._key)
            return false;

        return (e._mods & binding.required_mods) == binding.required_mods;
    }
} // namespace carrot::input
