//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ActionMap.h"
#include "InputActionCatalog.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::input {
    struct input_action_binding_summary_t
    {
        std::optional<input_action_definition_t> definition;
        input_action_handle_t action{ };
        std::vector<action_binding_t> active_bindings;
        std::vector<action_binding_t> default_bindings;
        bool is_user_modified{ false };
        bool is_using_defaults{ true };
    };

    struct input_binding_conflict_t
    {
        std::optional<input_action_definition_t> definition;
        input_action_handle_t action{ };
        action_binding_t binding;
    };

    class input_binding_store_t
    {
    public:
        [[nodiscard]] bool initialize(const io::virtual_file_system_t& vfs,
                                      input_action_map_t& target_actions,
                                      std::string_view defaults_virtual_path,
                                      std::string_view user_virtual_path,
                                      input_action_catalog_view_t catalog = {});
        [[nodiscard]] bool reload();
        [[nodiscard]] bool save_user_bindings() const;
        [[nodiscard]] bool reset_to_defaults();
        [[nodiscard]] bool restore_action_defaults(input_action_handle_t action, bool persist = false);
        [[nodiscard]] bool set_action_bindings(input_action_handle_t action,
                                               std::vector<action_binding_t> bindings,
                                               bool persist = false);
        [[nodiscard]] bool add_action_binding(input_action_handle_t action,
                                              action_binding_t binding,
                                              bool persist = false);
        [[nodiscard]] bool is_action_user_modified(input_action_id_t action) const;
        [[nodiscard]] std::optional<input_action_binding_summary_t> describe_action(input_action_handle_t action) const;
        [[nodiscard]] std::vector<input_action_binding_summary_t> describe_actions() const;
        [[nodiscard]] std::vector<input_binding_conflict_t> find_conflicts(const action_binding_t& binding,
                                                                           input_action_id_t exclude_action = {}) const;

        [[nodiscard]] bool has_user_bindings() const noexcept { return _loaded_user_bindings; }
        [[nodiscard]] std::string_view defaults_virtual_path() const noexcept { return _defaults_virtual_path; }
        [[nodiscard]] std::string_view user_virtual_path() const noexcept { return _user_virtual_path; }
        [[nodiscard]] const input_action_map_t& default_actions() const noexcept { return _default_actions; }
        [[nodiscard]] input_action_catalog_view_t catalog() const noexcept { return _catalog; }

    private:
        [[nodiscard]] bool load_defaults();
        [[nodiscard]] bool write_string_to_virtual_path(std::string_view virtual_path, std::string_view contents) const;
        [[nodiscard]] static bool bindings_equal(const action_binding_t& lhs, const action_binding_t& rhs) noexcept;
        [[nodiscard]] static bool binding_inputs_equal(const action_binding_t& lhs, const action_binding_t& rhs) noexcept;
        [[nodiscard]] static bool binding_lists_equal(std::span<const action_binding_t> lhs,
                                                      std::span<const action_binding_t> rhs) noexcept;
        [[nodiscard]] input_action_binding_summary_t build_summary(input_action_handle_t action) const;

        const io::virtual_file_system_t* _vfs{ nullptr };
        input_action_map_t* _target_actions{ nullptr };
        std::string _defaults_virtual_path;
        std::string _user_virtual_path;
        input_action_map_t _default_actions;
        input_action_catalog_view_t _catalog;
        bool _loaded_user_bindings{ false };
    };
} // namespace carrot::input
