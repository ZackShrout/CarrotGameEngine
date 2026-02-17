//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Voice/VoiceHandle.h"

namespace carrot::audio {
    /**
     * @brief Audio event types sent from the audio thread to the engine thread.
     *
     * Audio events communicate state changes that occur during real-time
     * audio processing back to the non-real-time side of the engine.
     *
     * Events are generated exclusively on the audio thread and consumed
     * by the audio module or engine thread.
     */
    enum class audio_event_type : uint8_t
    {
        /**
         * @brief A voice has finished playback and is no longer active.
         *
         * This event is emitted when a voice transitions to an idle state
         * after completing playback or finishing its release envelope.
         *
         * Upon receiving this event, the engine thread may safely:
         *  - release or recycle the associated voice handle
         *  - update bookkeeping or gameplay state
         */
        voice_finished,
    };

    /**
     * @brief Single audio event.
     *
     * Audio events are lightweight, trivially copyable messages sent from
     * the audio thread to the engine thread to report changes in voice state.
     *
     * Events must be:
     *  - allocation-free
     *  - fixed size
     *  - safe to enqueue from the real-time audio thread
     *
     * @note
     * The included voice handle may be stale by the time the event is
     * processed. Consumers must validate the handle before acting on it.
     */
    struct audio_event_t
    {
        /** Type of audio event. */
        audio_event_type type{ audio_event_type::voice_finished };

        /** Handle of the voice associated with the event. */
        voice_handle_t handle;
    };
} // namespace carrot::audio