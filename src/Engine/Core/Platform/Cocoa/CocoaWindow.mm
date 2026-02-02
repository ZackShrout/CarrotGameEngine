//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "CocoaWindow.h"

#include "AppDelegate.h"

// Note: even though some of these includes appear to be unused, they must be included
//       for private definitions needed for metal-cpp that are set in CMakeLists.txt
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <AppKit/AppKit.h>


namespace carrot::core::platform {
    cocoa_window_t::cocoa_window_t(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
        _width = width;
        _height = height;

        app_delegate_info_t info;
        info._window_rect = CGRectMake(0.0, 0.0, static_cast<CGFloat>(_width), static_cast<CGFloat>(_height));
        info._window_title = title;

        _controller = (void *)[[app_delegate_t class] alloc];
        _controller = (void *)[(id)_controller initWithInfo:info];

        LOG_CORE_INFO("Creating window with size {}x{}, title \"{}\"", width, height, title);

        NSApplication *app = [NSApplication sharedApplication];
        [app setDelegate:(id<NSApplicationDelegate>)_controller];

        //[app run];

        [app finishLaunching];
    }

    cocoa_window_t::~cocoa_window_t() noexcept
    {
        if (_controller)
        {
            id delegate = (id)_controller;
            [delegate release];
        }
    }

    void cocoa_window_t::poll_events() noexcept
    {
        @autoreleasepool {
            NSApplication *app = [NSApplication sharedApplication];

            // Non-blocking version (recommended for game loop)
            // Use [NSDate distantPast] or nil to drain current queue only
            while (NSEvent *event = [app nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:nil
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES])
            {
                [app sendEvent:event];

                // Add translation here later
                // Example:
                // NSEventType type = [event type];
                // if (type == NSEventTypeKeyDown) { ... }
            }
        }

    // After events: your engine can now update/render/present
    }

    void cocoa_window_t::set_should_close(bool should_close) noexcept
    {

    }

    native_window_handle_t cocoa_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };

        handle.cocoa_t.ns_window = nullptr;
        handle.cocoa_t.metal_layer = nullptr;

        return handle;
    }
} // namespace carrot::core::plaform