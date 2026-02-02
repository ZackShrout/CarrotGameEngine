//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "CocoaWindow.h"

#include "AppDelegate.h"
#include "Events/Events.h"
#include "CocoaInputUtils.h"

// Note: even though some of these includes appear to be unused, they must be included
//       for private definitions needed for metal-cpp that are set in CMakeLists.txt
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>


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
        _ns_window = [(id)_controller createAndReturnWindow];

        LOG_CORE_INFO("Creating window with size {}x{}, title \"{}\"", width, height, title);

        NSApplication *app = [NSApplication sharedApplication];
        [app setDelegate:(id<NSApplicationDelegate>)_controller];

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

            for (;;)
            {
                @autoreleasepool {
                    NSEvent *event = [app nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:nil
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                    if (!event) break;

                    [app sendEvent:event];

                    NSEventType type = [event type];
                    switch (type)
                    {
                        case NSEventTypeKeyDown:
                        case NSEventTypeKeyUp:
                        {
                            unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];
                            // or use [event keyCode] directly if you want scan codes

                            carrot::events::key_event_t e{};
                            e._key    = carrot::input::to_carrot_key([event keyCode]);
                            e._action = (type == NSEventTypeKeyDown) ? events::key_action::press : events::key_action::release;
                            e._repeat = [event isARepeat];
                            e._mods   = translate_modifier_flags([event modifierFlags]);

                            _on_key.broadcast(e); // your multicast
                            break;
                        }

                        case NSEventTypeLeftMouseDown:
                        case NSEventTypeLeftMouseUp:
                        {
                            auto loc = [event locationInWindow];
                            carrot::events::mouse_button_event_t e{};
                            e._button = input::mouse_button::left;
                            e._action = (type == NSEventTypeLeftMouseDown) ? events::key_action::press : events::key_action::release;
                            e._pos    = { (float)loc.x, (float)(_height - loc.y) }; // flip Y ?
                            _on_mouse_button.broadcast(e);
                            break;
                        }

                        case NSEventTypeRightMouseDown:
                        case NSEventTypeRightMouseUp:
                        {
                            auto loc = [event locationInWindow];
                            carrot::events::mouse_button_event_t e{};
                            e._button = input::mouse_button::right;
                            e._action = (type == NSEventTypeRightMouseDown) ? events::key_action::press : events::key_action::release;
                            e._pos    = { (float)loc.x, (float)(_height - loc.y) };
                            _on_mouse_button.broadcast(e);
                            break;
                        }

                        case NSEventTypeMouseMoved:
                        case NSEventTypeLeftMouseDragged:
                        {
                            auto loc = [event locationInWindow];
                            // You usually need to keep last position yourself to compute delta
                            carrot::events::mouse_moved_event_t e{};
                            e._pos   = { (float)loc.x, (float)loc.y };
                            e._delta = { (float)[event deltaX], (float)[event deltaY] };
                            _on_mouse_moved.broadcast(e);
                            break;
                        }

                        case NSEventTypeScrollWheel:
                        {
                            carrot::events::mouse_scrolled_event_t e{};
                            e._delta = { (float)[event scrollingDeltaX], (float)[event scrollingDeltaY] };
                            _on_mouse_scrolled.broadcast(e);
                            break;
                        }

                        // Add more: other mouse, flagsChanged (modifiers only), etc.

                        default:
                            break;
                    }
                }
            }

            NSEventModifierFlags currentFlags = [app currentEvent].modifierFlags;

            if (currentFlags != _last_modifier_flags)
            {
                NSEventModifierFlags changed{ currentFlags ^ _last_modifier_flags };

                auto send_mod_event{ [&](carrot::input::key_code key, bool pressed) {
                    carrot::events::key_event_t e{ };
                    e._key = key;
                    e._action = pressed ? events::key_action::press : events::key_action::release;
                    e._repeat = false;
                    e._mods = carrot::core::platform::translate_modifier_flags(currentFlags);

                    _on_key.broadcast(e);
                } };

                if (changed & NSEventModifierFlagShift)
                {
                    bool pressed{ (currentFlags & NSEventModifierFlagShift) != 0 };
                    send_mod_event(carrot::input::key_code::left_shift, pressed);
                }
                if (changed & NSEventModifierFlagControl)
                {
                    bool pressed{ (currentFlags & NSEventModifierFlagControl) != 0 };
                    send_mod_event(carrot::input::key_code::left_control, pressed);
                }
                if (changed & NSEventModifierFlagOption)
                {
                    bool pressed{ (currentFlags & NSEventModifierFlagOption) != 0 };
                    send_mod_event(carrot::input::key_code::left_alt, pressed);
                }
                if (changed & NSEventModifierFlagCommand)
                {
                    bool pressed{ (currentFlags & NSEventModifierFlagCommand) != 0 };
                    send_mod_event(carrot::input::key_code::left_super, pressed);
                }

                _last_modifier_flags = currentFlags;
            }
        }
    }

    void cocoa_window_t::set_should_close(bool should_close) noexcept
    {
        if (should_close)
        {
            NSApplication *app = [NSApplication sharedApplication];
            [app terminate:nil];
        }
    }

    native_window_handle_t cocoa_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };

        handle.cocoa_t.ns_window = _ns_window;
        handle.cocoa_t.metal_layer = nullptr;

        return handle;
    }
} // namespace carrot::core::plaform