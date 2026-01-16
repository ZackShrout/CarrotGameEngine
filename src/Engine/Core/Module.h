//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

#include <string_view>

namespace carrot::core {
    /**
     * @brief Abstract base class for major engine subsystems with explicit lifecycle.
     *
     * Every significant subsystem that:
     *   - owns heavyweight resources,
     *   - requires controlled initialization/shutdown ordering,
     *   - participates in hot-reloading, or
     *   - needs per-frame/update callbacks
     *
     * should inherit from this interface.
     *
     * @note Implementations **must** follow a very strict lifecycle:
     *       1. constructor (lightweight!)
     *       2. init()       → allocate/create resources, load configs, subscribe to events...
     *       3. update loop  → zero or more early_update/update/late_update calls
     *       4. shutdown()   → release **everything**, unregister from systems
     *       5. destructor   (should be trivial after successful shutdown)
     *
     * @attention Failing to release resources in shutdown() will almost certainly
     *            cause leaks during hot-reload or engine shutdown.
     *
     * @warning Most methods are **not thread-safe** unless explicitly documented otherwise.
     *          The usual pattern is that all lifecycle methods are called from the main/engine thread.
     */
    class module_t
    {
    public:
        module_t() = default;
        virtual ~module_t() = default;

        DISABLE_COPY(module_t)

        /**
         * @brief One-time initialization called during engine startup.
         *
         * This is where subsystems should:
         *  - allocate heavyweight resources
         *  - load configuration
         *  - register callbacks/listeners
         *  - initialize third-party libraries they depend on
         *  - perform any other expensive one-time setup
         *
         * @note Will be called **exactly once** per subsystem lifetime
         *       (unless hot-reload is used)
         *
         * @post `_is_initialized` should be `true` when method returns successfully
         */
        virtual void init() = 0;

        /**
         * @brief Clean-up called during engine shutdown or before hot-reload.
         *
         * Must release **all** owned resources, unregister callbacks,
         * shut down dependent systems, etc.
         *
         * @note Will be called **exactly once** after init() (except in case of crash)
         * @note After successful shutdown(), the object should be in a state where
         *       init() can be called again (useful for hot-reload)
         *
         * @post `_is_initialized` should be `false` when method returns
         */
        virtual void shutdown() = 0;

        // ── Optional per-frame callbacks ─────────────────────────────────────

        /**
         * @brief Early per-frame update - called before most other systems
         *
         * Usually used for:
         *  - input polling / preprocessing
         *  - animation state advance
         *  - physics prediction
         *
         * @param delta_time Time elapsed since last frame (seconds)
         */
        virtual void early_update(const float delta_time) noexcept { (void)delta_time; }

        /**
         * @brief Main per-frame update - the "normal" update position
         *
         * Most gameplay/AI/logic/update work should go here.
         *
         * @param delta_time Time elapsed since last frame (seconds)
         */
        virtual void update(const float delta_time) noexcept { (void)delta_time; }

        /**
         * @brief Late per-frame update - called after most other systems
         *
         * Commonly used for:
         *  - post-processing
         *  - camera finalization
         *  - debug drawing preparation
         *  - collecting statistics
         *
         * @param delta_time Time elapsed since last frame (seconds)
         */
        virtual void late_update(const float delta_time) noexcept { (void)delta_time; }

        /**
         * @brief Returns human-readable name of the subsystem
         *
         * Used extensively for:
         *  - logging
         *  - debug UI
         *  - profiling markers
         *  - hot-reload messages
         *
         * @return Short, unique identifier of the module (never empty)
         */
        [[nodiscard]] virtual std::string_view get_name() const noexcept = 0;

    protected:
        /// @brief Tracks whether init() has been successfully called
        ///        (and shutdown() not yet called)
        bool _is_initialized{ false };
    };

    /**
     * @def CARROT_MODULE_NAME(Name)
     * @brief One-line convenience macro for implementing get_name()
     *
     * Usage example:
     * @code
     * class AudioSystem final : public module_t {
     *     CARROT_MODULE_NAME("Audio")
     *     // ...
     * };
     * @endcode
     */
#define CARROT_MODULE_NAME(Name) \
    [[nodiscard]] std::string_view get_name() const noexcept override { return Name; }
} // namespace carrot::core
