//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "CocoaWindow.h"

#include "AppDelegate.h"
#include "Events/Events.h"
#include "CocoaInputUtils.h"

#include <AppKit/AppKit.h>

namespace carrot::core::platform {
    cocoa_window_t::cocoa_window_t(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
        _width = width;
        _height = height;

        app_delegate_info_t info;
        info._window_rect = CGRectMake(0.0, 0.0, static_cast<CGFloat>(_width), static_cast<CGFloat>(_height));
        info._window_title = title;

        _controller = static_cast<void*>([[app_delegate_t class] alloc]);
        _controller = static_cast<void*>([(id)_controller initWithInfo:info owner:this]);
        _ns_window = [(id)_controller createAndReturnWindow];

        LOG_CORE_INFO("Creating window with size {}x{}, title \"{}\"", width, height, title);

        NSApplication* app{ [NSApplication sharedApplication] };
        [app setDelegate:(id<NSApplicationDelegate>)_controller];

        [app finishLaunching];
    }

    cocoa_window_t::~cocoa_window_t() noexcept
    {
        if (_controller)
        {
            id delegate{ (id)_controller };
            [delegate release];
        }
    }

    void cocoa_window_t::poll_events() noexcept
    {
        @autoreleasepool
        {
            NSApplication* app{ [NSApplication sharedApplication] };

            for (;;)
            {
                @autoreleasepool
                {
                    NSEvent* event{ [app nextEventMatchingMask:NSEventMaskAny
                                         untilDate:[NSDate distantPast]
                                         inMode:NSDefaultRunLoopMode
                                         dequeue:YES] };
                    if (!event) break;

                    [app sendEvent:event];

                    NSEventType type{ [event type] };
                    switch (type)
                    {
                        case NSEventTypeKeyDown:
                        case NSEventTypeKeyUp:
                        {
                            events::key_event_t e{ };
                            e._key = input::to_carrot_key([event keyCode]);
                            e._action = (type == NSEventTypeKeyDown) ? events::key_action::press : events::key_action::release;
                            e._repeat = [event isARepeat];
                            e._mods = translate_modifier_flags([event modifierFlags]);

                            _on_key.broadcast(e);
                            break;
                        }

                        case NSEventTypeLeftMouseDown:
                        case NSEventTypeLeftMouseUp:
                        case NSEventTypeRightMouseDown:
                        case NSEventTypeRightMouseUp:
                        case NSEventTypeOtherMouseDown:
                        case NSEventTypeOtherMouseUp:
                        {
                            NSInteger button_num{ [event buttonNumber] };
                            NSPoint loc{ [event locationInWindow] };

                            bool is_press{ (type == NSEventTypeLeftMouseDown  ||
                                            type == NSEventTypeRightMouseDown ||
                                            type == NSEventTypeOtherMouseDown) };

                            events::mouse_button_event_t e{ };
                            e._button = input::to_carrot_mouse_button(button_num);
                            e._action = is_press ? events::key_action::press : events::key_action::release;
                            e._pos = { static_cast<float>(loc.x), static_cast<float>(_height - loc.y) };

                            _on_mouse_button.broadcast(e);
                            break;
                        }

                        case NSEventTypeMouseMoved:
                        case NSEventTypeLeftMouseDragged:
                        case NSEventTypeRightMouseDragged:
                        case NSEventTypeOtherMouseDragged:
                        {
                            float dx{ static_cast<float>([event deltaX]) };
                            float dy{ static_cast<float>([event deltaY]) };
                            NSPoint loc{ [event locationInWindow] };

                            events::mouse_moved_event_t e{ };
                            e._delta = { dx, dy };
                            e._pos = { static_cast<float>(loc.x), static_cast<float>(_height - loc.y) };

                            _on_mouse_moved.broadcast(e);

                            _last_mouse_position = e._pos;
                            break;
                        }

                        case NSEventTypeScrollWheel:
                        {
                            events::mouse_scrolled_event_t e{ };
                            e._delta = { static_cast<float>([event scrollingDeltaX]), static_cast<float>([event scrollingDeltaY]) };

                            _on_mouse_scrolled.broadcast(e);
                            break;
                        }

                        default:
                            break;
                    }
                }
            }

            NSEventModifierFlags currentFlags{ [app currentEvent].modifierFlags };

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
//        if (should_close)
//        {
//            NSApplication* app{ [NSApplication sharedApplication] };
//            [app terminate:nil];
//        }
        _should_close = should_close;
    }

    void cocoa_window_t::set_fullscreen(const bool fullscreen) noexcept
    {
        if (!_ns_window) return;
        if (_is_fullscreen == fullscreen) return;
        
        NSWindow* window{ (NSWindow*)_ns_window };
        
        @autoreleasepool {
            [window toggleFullScreen:nil];
        }
    }

    native_window_handle_t cocoa_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };

        handle.cocoa_t.ns_window = _ns_window;
        handle.cocoa_t.metal_layer = _metal_layer;

        return handle;
    }

    void cocoa_window_t::update_size(const uint32_t width, const uint32_t height) noexcept
    {
        if (width == _width && height == _height) return;

        _width = width;
        _height = height;

        _on_window_resized.broadcast({ _width, _height });
    }
} // namespace carrot::core::plaform
