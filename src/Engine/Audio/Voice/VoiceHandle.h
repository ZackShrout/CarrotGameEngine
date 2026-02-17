//
// Created by Zack Shrout on 2/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Opaque handle identifying a single active audio voice instance.
     *
     * A voice handle uniquely identifies a specific playback instance of an
     * audio asset. Handles are returned by audio playback functions and may be
     * used to control playback (pause, resume, stop) after creation.
     *
     * Internally, a voice handle consists of an index into the audio engine's
     * voice table and a generation counter used to detect stale handles.
     *
     * Voice handles are lightweight, trivially copyable, and safe to store by
     * value. They may become invalid automatically when the associated voice
     * finishes playback or is stopped.
     *
     * @note
     * Operations performed on an invalid or stale voice handle have no effect.
     * This behavior is intentional and allows safe, fire-and-forget usage.
     */
    struct voice_handle_t
    {
        /** Index into the audio engine's voice table. */
        uint32_t index{ 0 };

        /** Generation counter used to detect stale handles. */
        uint32_t generation{ 0 };

        /**
         * @brief Checks whether this handle refers to a currently valid voice.
         *
         * A handle is considered valid if its generation is non-zero. Validity
         * does not guarantee that the voice is currently audible; it only
         * indicates that the handle refers to a live voice instance.
         *
         * @return True if the handle is valid, false otherwise.
         */
        [[nodiscard]] constexpr bool is_valid() const noexcept
        {
            return generation != 0;
        }

        /**
         * @brief Returns an invalid voice handle.
         *
         * An invalid handle does not refer to any voice instance. This value is
         * returned when playback fails or an asset cannot be resolved.
         *
         * @return An invalid voice handle.
         */
        static constexpr voice_handle_t invalid() noexcept
        {
            return { };
        }
    };
}
