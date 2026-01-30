//
// Created by Zack Shrout on 1/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "ShaderWatcher.h"

#include "Core/Logger.h"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>  // For dispatch_queue_t, dispatch_async, etc.
#include <string>

namespace carrot::hot_reload {

namespace {

struct state_t
{
    FSEventStreamRef     stream    = nullptr;
    dispatch_queue_t     queue     = nullptr;
    bool                 initialized = false;
} _state;

// Callback: runs on the dispatch queue's thread
void fsevents_callback(
    [[maybe_unused]] ConstFSEventStreamRef streamRef,
    [[maybe_unused]] void* clientCallBackInfo,
    size_t numEvents,
    void* eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    [[maybe_unused]] const FSEventStreamEventId eventIds[])
{
    char** paths = static_cast<char**>(eventPaths);

    for (size_t i = 0; i < numEvents; ++i)
    {
        std::string full_path{ paths[i] };
        FSEventStreamEventFlags flags{ eventFlags[i] };

        if (!(flags & (kFSEventStreamEventFlagItemModified |
                       kFSEventStreamEventFlagItemCreated |
                       kFSEventStreamEventFlagItemRenamed |
                       kFSEventStreamEventFlagItemRemoved)))
            continue;

        // Extract just the filename (FSEvents gives full path)
        size_t last_slash = full_path.find_last_of("/\\");
        if (last_slash == std::string::npos) continue;
        std::string filename = full_path.substr(last_slash + 1);

        // Filter to relevant shaders
        if (!filename.ends_with(".vert.hlsl") && !filename.ends_with(".frag.hlsl") &&
            !filename.ends_with(".comp.hlsl"))
            continue;

        // Post the actual work to main thread (safe for logging / compilation / Vulkan)
        // Use a copy of filename since lambda captures by value
        dispatch_async(dispatch_get_main_queue(), [filename = std::move(filename)] {
            shader_watcher_t::try_compile_and_notify(filename);
        });
    }
}

} // anonymous namespace

void shader_watcher_t::init(const shader_reload_callback_t& callback) noexcept
{
    _callback = callback;

#ifndef CARROT_SOURCE_ROOT
#error "CARROT_SOURCE_ROOT must be defined"
#endif

    std::string shader_dir{ std::string(CARROT_SOURCE_ROOT) + "/shaders" };

    CFStringRef cf_path = CFStringCreateWithCString(nullptr, shader_dir.c_str(), kCFStringEncodingUTF8);
    if (!cf_path) return;

    CFArrayRef paths_to_watch = CFArrayCreate(nullptr, (const void**)&cf_path, 1, &kCFTypeArrayCallBacks);
    CFRelease(cf_path);

    if (!paths_to_watch) return;

    CFAbsoluteTime latency = 0.05;  // 50 ms coalescing

    FSEventStreamContext context = {0, nullptr, nullptr, nullptr, nullptr};

    _state.stream = FSEventStreamCreate(
        nullptr,
        &fsevents_callback,
        &context,
        paths_to_watch,
        kFSEventStreamEventIdSinceNow,
        latency,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer
    );

    CFRelease(paths_to_watch);

    if (!_state.stream)
    {
        LOG_CORE_ERROR("FSEventStreamCreate failed for {}", shader_dir);
        return;
    }

    _state.queue = dispatch_queue_create("com.bunnysoft.carrot.shaderwatcher.fsevents", DISPATCH_QUEUE_SERIAL);
    if (!_state.queue)
    {
        LOG_CORE_ERROR("dispatch_queue_create failed");
        FSEventStreamInvalidate(_state.stream);
        FSEventStreamRelease(_state.stream);
        _state.stream = nullptr;
        return;
    }

    FSEventStreamSetDispatchQueue(_state.stream, _state.queue);

    if (!FSEventStreamStart(_state.stream))
    {
        LOG_CORE_ERROR("FSEventStreamStart failed");
        dispatch_release(_state.queue);
        _state.queue = nullptr;
        FSEventStreamInvalidate(_state.stream);
        FSEventStreamRelease(_state.stream);
        _state.stream = nullptr;
        return;
    }

    LOG_CORE_INFO("Shader hot-reload watching (macOS/FSEvents dispatch queue): {}", shader_dir);
    _state.initialized = true;
}

void shader_watcher_t::shutdown() noexcept
{
    if (_state.stream)
    {
        FSEventStreamStop(_state.stream);
        FSEventStreamSetDispatchQueue(_state.stream, nullptr);  // Unschedule
        FSEventStreamInvalidate(_state.stream);
        FSEventStreamRelease(_state.stream);
        _state.stream = nullptr;
    }

    if (_state.queue)
    {
        dispatch_release(_state.queue);
        _state.queue = nullptr;
    }

    _state.initialized = false;
    _callback = nullptr;
}

void shader_watcher_t::poll() noexcept
{
    // Nothing needed — events are delivered asynchronously via dispatch queue
}

} // namespace carrot::hot_reload