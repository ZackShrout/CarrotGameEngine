//
// Created by Zack Shrout on 2/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::assets {
    class asset_manager_t;

    /**
     * @brief Static service locator for accessing asset registry instances.
     *
     * This class provides a clean, global point of access to any of the active asset registries
     * without requiring direct engine references in game code, scripting, or systems.
     *
     * The engine is responsible for registering the instances exactly once during startup
     * via @ref provide() and clearing it during shutdown via @ref reset().
     *
     * Typical usage:
     * @code
     * auto& registry = assets::asset_service_t::audio();
     * const auto id = assets::make_asset_id(asset_name);
     * const auto handle = registry.find(id);
     * @endcode
     *
     * Thread-safety note: This is **not** thread-safe for concurrent registration/reset.
     * Registration/reset should only occur from the main/engine thread during init/shutdown.
     */
    class asset_service_t
    {
    public:
        static void provide(asset_manager_t* manager) noexcept;
        static void reset() noexcept;

        [[nodiscard]] static asset_manager_t& manager();
        [[nodiscard]] static asset_manager_t* try_manager() noexcept;

    private:
        inline static asset_manager_t* _manager{ nullptr };
    };
} // namespace carrot::assets
