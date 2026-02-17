//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::audio {
    class audio_module_t;

    /**
     * @brief Static service locator for accessing the audio module instance.
     *
     * This class provides a clean, global point of access to the active @ref audio_module_t
     * without requiring direct engine references in game code, scripting, or systems.
     *
     * The engine is responsible for registering the instance exactly once during startup
     * via @ref provide() and clearing it during shutdown via @ref reset().
     *
     * Typical usage:
     * @code
     * auto& audio = audio_service_t::get();
     * audio.play_one_shot("sfx/explosion", position, volume);
     * @endcode
     *
     * Thread-safety note: This is **not** thread-safe for concurrent registration/reset.
     * Registration/reset should only occur from the main/engine thread during init/shutdown.
     */
    class audio_service_t
    {
    public:
        /**
         * @brief Registers the audio module instance with the service locator.
         *
         * Should be called **exactly once** by the engine during initialization,
         * typically right after constructing the @ref audio_module_t.
         *
         * If called multiple times, logs a fatal error (programming mistake).
         * The pointer is updated defensively to avoid undefined behavior.
         *
         * @param instance Pointer to the live audio_module_t instance.
         *                 Must remain valid until reset() is called.
         */
        static void provide(audio_module_t* instance) noexcept;

        /**
         * @brief Retrieves the active audio module instance (reference).
         *
         * This is the primary access point for most game code.
         *
         * @note In debug builds: asserts + fatal log if no instance is provided.
         *       In release builds: fatal log only (no assert).
         *
         * @warning This function is intended to be called from the engine thread
         *          or systems synchronized with it. It must not be called from
         *          the real-time audio thread.
         *
         * @return Reference to the active @ref audio_module_t.
         */
        [[nodiscard]] static audio_module_t& get();

        /**
         * @brief Retrieves the active audio module instance (pointer, nullable).
         *
         * Safe to call at any time; returns `nullptr` if no instance has been provided
         * (e.g. headless mode, dedicated server, audio disabled, before init, after shutdown).
         *
         * Preferred when audio is genuinely optional.
         *
         * @return Pointer to the active @ref audio_module_t, or `nullptr` if unavailable.
         */
        [[nodiscard]] static audio_module_t* try_get() noexcept { return _instance; }

        /**
         * @brief Clears the registered audio module instance.
         *
         * Should be called by the engine during shutdown (or subsystem reload).
         *
         * After this call, @ref get() will log/assert and @ref try_get() will return `nullptr`.
         */
        static void reset() noexcept { _instance = nullptr; }

        /**
         * @brief Checks whether an audio module instance has been provided.
         *
         * Useful for optional systems or logging purposes.
         *
         * @return `true` if @ref provide() has been called and @ref reset() has not yet been called.
         */
        [[nodiscard]] static bool is_provided() noexcept { return _instance != nullptr; }

    private:
        inline static audio_module_t* _instance{ nullptr };
    };
} // namespace carrot::audio
