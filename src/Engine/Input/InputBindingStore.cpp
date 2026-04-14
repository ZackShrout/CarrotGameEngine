//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "InputBindingStore.h"

#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"

namespace carrot::input {
    namespace {
        [[nodiscard]] std::optional<std::filesystem::path> resolve_virtual_path(const io::virtual_file_system_t& vfs,
                                                                                const std::string_view virtual_path) noexcept
        {
            if (virtual_path.empty())
                return std::nullopt;

            if (const std::filesystem::path fs_path{ virtual_path }; fs_path.is_absolute())
                return fs_path.lexically_normal();

            const size_t scheme_pos{ virtual_path.find("://") };
            if (scheme_pos == std::string_view::npos || scheme_pos == 0u)
                return std::nullopt;

            const std::string_view scheme{ virtual_path.substr(0u, scheme_pos) };
            const std::string_view remainder{ virtual_path.substr(scheme_pos + 3u) };
            const std::optional<io::vfs_mount_point_t> mount{ vfs.get_mount(scheme) };
            if (!mount.has_value())
                return std::nullopt;

            return (mount->root / remainder).lexically_normal();
        }
    } // namespace

    bool input_binding_store_t::initialize(const io::virtual_file_system_t& vfs,
                                           input_action_map_t& target_actions,
                                           const std::string_view defaults_virtual_path,
                                           const std::string_view user_virtual_path,
                                           const input_action_catalog_view_t catalog)
    {
        _vfs = &vfs;
        _target_actions = &target_actions;
        _defaults_virtual_path = std::string{ defaults_virtual_path };
        _user_virtual_path = std::string{ user_virtual_path };
        _catalog = catalog;
        _loaded_user_bindings = false;

        if (!load_defaults())
            return false;

        const std::optional<std::filesystem::path> user_native_path{ resolve_virtual_path(vfs, _user_virtual_path) };
        if (user_native_path.has_value() && std::filesystem::exists(*user_native_path))
        {
            if (_target_actions->load_bindings_from_file(vfs, _user_virtual_path))
            {
                _loaded_user_bindings = true;
                return true;
            }

            LOG_CORE_WARN("Falling back to default input bindings after failing to load persisted bindings from '{}'",
                          _user_virtual_path);
        }

        *_target_actions = _default_actions;
        return true;
    }

    bool input_binding_store_t::reload()
    {
        if (!_vfs || !_target_actions)
            return false;

        return initialize(*_vfs, *_target_actions, _defaults_virtual_path, _user_virtual_path, _catalog);
    }

    bool input_binding_store_t::save_user_bindings() const
    {
        if (!_vfs || !_target_actions || _user_virtual_path.empty())
            return false;

        const std::string serialized{ _target_actions->serialize_bindings_to_json() };
        return write_string_to_virtual_path(_user_virtual_path, serialized);
    }

    bool input_binding_store_t::reset_to_defaults()
    {
        if (!_target_actions)
            return false;

        *_target_actions = _default_actions;
        _loaded_user_bindings = false;
        return true;
    }

    bool input_binding_store_t::restore_action_defaults(const input_action_handle_t action, const bool persist)
    {
        if (!_target_actions || !action)
            return false;

        _target_actions->set_bindings_for_action(action, _default_actions.bindings_for_action(action.id));
        _loaded_user_bindings = true;
        return !persist || save_user_bindings();
    }

    bool input_binding_store_t::set_action_bindings(const input_action_handle_t action,
                                                    std::vector<action_binding_t> bindings,
                                                    const bool persist)
    {
        if (!_target_actions || !action)
            return false;

        _target_actions->set_bindings_for_action(action, std::move(bindings));
        _loaded_user_bindings = true;
        return !persist || save_user_bindings();
    }

    bool input_binding_store_t::add_action_binding(const input_action_handle_t action,
                                                   action_binding_t binding,
                                                   const bool persist)
    {
        if (!_target_actions || !action)
            return false;

        binding.action = std::string{ action.authored_id };
        binding.action_id = action.id;
        _target_actions->add_binding(std::move(binding));
        _loaded_user_bindings = true;
        return !persist || save_user_bindings();
    }

    bool input_binding_store_t::is_action_user_modified(const input_action_id_t action) const
    {
        if (!_target_actions || !action.valid())
            return false;

        return !binding_lists_equal(_target_actions->bindings_for_action(action), _default_actions.bindings_for_action(action));
    }

    std::optional<input_action_binding_summary_t> input_binding_store_t::describe_action(const input_action_handle_t action) const
    {
        if (!_target_actions || !action)
            return std::nullopt;

        return build_summary(action);
    }

    std::vector<input_action_binding_summary_t> input_binding_store_t::describe_actions() const
    {
        std::vector<input_action_binding_summary_t> summaries;
        if (!_target_actions)
            return summaries;

        if (!_catalog.empty())
        {
            summaries.reserve(_catalog.definitions().size());
            for (const input_action_definition_t& definition : _catalog.definitions())
                summaries.push_back(build_summary(definition.action));
            return summaries;
        }

        std::vector<input_action_handle_t> seen_actions;
        seen_actions.reserve(_target_actions->bindings().size());
        for (const action_binding_t& binding : _target_actions->bindings())
        {
            const auto duplicate_it{ std::find_if(seen_actions.begin(), seen_actions.end(), [&binding](const input_action_handle_t action)
            {
                return action.id == binding.action_id;
            }) };
            if (duplicate_it != seen_actions.end())
                continue;

            seen_actions.push_back(input_action_handle_t{
                .id = binding.action_id,
                .authored_id = binding.action
            });
        }

        summaries.reserve(seen_actions.size());
        for (const input_action_handle_t action : seen_actions)
            summaries.push_back(build_summary(action));

        return summaries;
    }

    std::vector<input_binding_conflict_t> input_binding_store_t::find_conflicts(const action_binding_t& binding,
                                                                                const input_action_id_t exclude_action) const
    {
        std::vector<input_binding_conflict_t> conflicts;
        if (!_target_actions)
            return conflicts;

        for (const action_binding_t& candidate : _target_actions->bindings())
        {
            if (!candidate.action_id.valid() || candidate.action_id == exclude_action)
                continue;

            if (!binding_inputs_equal(candidate, binding))
                continue;

            const std::optional<input_action_definition_t> definition{ _catalog.find(candidate.action_id) };
            conflicts.push_back(input_binding_conflict_t{
                .definition = definition,
                .action = input_action_handle_t{
                    .id = candidate.action_id,
                    .authored_id = definition.has_value() ? definition->action.authored_id : std::string_view{ candidate.action }
                },
                .binding = candidate
            });
        }

        return conflicts;
    }

    bool input_binding_store_t::load_defaults()
    {
        if (!_vfs || _defaults_virtual_path.empty())
            return false;

        input_action_map_t loaded_defaults;
        if (!loaded_defaults.load_bindings_from_file(*_vfs, _defaults_virtual_path))
            return false;

        _default_actions = std::move(loaded_defaults);
        return true;
    }

    bool input_binding_store_t::write_string_to_virtual_path(const std::string_view virtual_path,
                                                             const std::string_view contents) const
    {
        if (!_vfs)
            return false;

        const std::optional<std::filesystem::path> native_path{ resolve_virtual_path(*_vfs, virtual_path) };
        if (!native_path.has_value())
        {
            LOG_CORE_WARN("Input binding save path '{}' could not be resolved", virtual_path);
            return false;
        }

        std::vector<std::uint8_t> bytes(contents.begin(), contents.end());
        if (!utils::file::write_binary_file(*native_path, bytes))
        {
            LOG_CORE_WARN("Failed to write input bindings to '{}'", utils::file::to_log_string(*native_path));
            return false;
        }

        return true;
    }

    bool input_binding_store_t::bindings_equal(const action_binding_t& lhs, const action_binding_t& rhs) noexcept
    {
        return lhs.action_id == rhs.action_id &&
               binding_inputs_equal(lhs, rhs);
    }

    bool input_binding_store_t::binding_inputs_equal(const action_binding_t& lhs, const action_binding_t& rhs) noexcept
    {
        return lhs.type == rhs.type &&
               lhs.key == rhs.key &&
               lhs.required_mods == rhs.required_mods &&
               lhs.gamepad_button == rhs.gamepad_button &&
               lhs.gamepad_axis == rhs.gamepad_axis &&
               lhs.gamepad_axis_direction == rhs.gamepad_axis_direction &&
               std::fabs(lhs.gamepad_axis_threshold - rhs.gamepad_axis_threshold) <= 0.0001f;
    }

    bool input_binding_store_t::binding_lists_equal(const std::span<const action_binding_t> lhs,
                                                    const std::span<const action_binding_t> rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i{ 0u }; i < lhs.size(); ++i)
        {
            if (!bindings_equal(lhs[i], rhs[i]))
                return false;
        }

        return true;
    }

    input_action_binding_summary_t input_binding_store_t::build_summary(const input_action_handle_t action) const
    {
        const std::vector<action_binding_t> active_bindings{ _target_actions ? _target_actions->bindings_for_action(action.id)
                                                                              : std::vector<action_binding_t>{ } };
        const std::vector<action_binding_t> default_bindings{ _default_actions.bindings_for_action(action.id) };
        const bool is_user_modified{ !binding_lists_equal(active_bindings, default_bindings) };

        return input_action_binding_summary_t{
            .definition = _catalog.find(action.id),
            .action = action,
            .active_bindings = active_bindings,
            .default_bindings = default_bindings,
            .is_user_modified = is_user_modified,
            .is_using_defaults = !is_user_modified
        };
    }
} // namespace carrot::input
