//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::ui {
    class ui_module_t;

    /**
     * @brief Static service locator for accessing the ui module instance.
     *
     * This class provides a clean, global point of access to the active @ref ui_module_t
     * without requiring direct engine references in game code, scripting, or systems.
     *
     * The engine is responsible for registering the instance exactly once during startup
     * via @ref provide() and clearing it during shutdown via @ref reset().
     *
     * Typical usage:
     * @code
     * auto& ui = ui_service_t::get();
     * ui_root_widget_t* root = ui.get_root();
     * @endcode
     */
    class ui_service_t
    {
    public:
        /**
         * @brief Registers the ui module instance with the service locator.
         *
         * Should be called **exactly once** by the engine during initialization,
         * typically right after constructing the @ref ui_module_t.
         *
         * If called multiple times, logs a fatal error (programming mistake).
         * The pointer is updated defensively to avoid undefined behavior.
         *
         * @param instance Pointer to the live ui_module_t instance.
         *                 Must remain valid until reset() is called.
         */
        static void provide(ui_module_t* instance) noexcept;

        /**
         * @brief Retrieves the active ui module instance (reference).
         *
         * This is the primary access point for most game code.
         *
         * @note In debug builds: asserts + fatal log if no instance is provided.
         *       In release builds: fatal log only (no assert).
         *
         * @return Reference to the active @ref ui_module_t.
         */
        [[nodiscard]] static ui_module_t& get();

        /**
         * @brief Retrieves the active ui module instance (pointer, nullable).
         *
         * Safe to call at any time; returns `nullptr` if no instance has been provided
         * (e.g. headless mode, dedicated server, ui disabled, before init, after shutdown).
         *
         * Preferred when ui is genuinely optional.
         *
         * @return Pointer to the active @ref ui_module_t, or `nullptr` if unavailable.
         */
        [[nodiscard]] static ui_module_t* try_get() noexcept { return _instance; }

        /**
         * @brief Clears the registered ui module instance.
         *
         * Should be called by the engine during shutdown (or subsystem reload).
         *
         * After this call, @ref get() will log/assert and @ref try_get() will return `nullptr`.
         */
        static void reset() noexcept { _instance = nullptr; }

        /**
         * @brief Checks whether an ui module instance has been provided.
         *
         * Useful for optional systems or logging purposes.
         *
         * @return `true` if @ref provide() has been called and @ref reset() has not yet been called.
         */
        [[nodiscard]] static bool is_provided() noexcept { return _instance != nullptr; }

    private:
        inline static ui_module_t* _instance{ nullptr };
    };
} // namespace carrot::ui