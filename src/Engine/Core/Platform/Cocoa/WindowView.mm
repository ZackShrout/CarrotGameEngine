//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WindowView.h"

#include "CocoaInputUtils.h"
#include "Events/Events.h"

@implementation WindowView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        // Initialize your custom state here
        _previous_modifier_flags = 0;   // ← critical for flagsChanged: logic

        // Optional: other one-time setup
        self.wantsLayer = YES;                    // useful later for Metal
        // self.layer.backgroundColor = ...       // if you want a default bg
    }
    return self;
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)event
{
    // Optional: log for debugging
    // NSLog(@"keyDown: %@", event);

    // We will **not** call super here — we swallow the event
    // (prevents the "bonk" sound on unknown keys)
}

- (void)keyUp:(NSEvent *)event
{
    // same as above
}

- (void)flagsChanged:(NSEvent *)event
{
    NSEventModifierFlags current  = [event modifierFlags];
    NSEventModifierFlags changed  = current ^ _previous_modifier_flags;  // bits that toggled

    // Helper to broadcast your engine event
    auto sendKeyEvent = [&](carrot::input::key_code key, carrot::events::key_action action) {
        carrot::events::key_event_t e{};
        e._key    = key;
        e._action = action;
        e._repeat = NO;               // modifiers don't repeat in this sense
        e._mods   = carrot::core::platform::translate_modifier_flags(current);  // or current & ~changed, your choice

        // Assuming you expose a way to reach your cocoa_window_t or broadcast globally
        // (you might need to make _on_key a public property or use a shared event bus)
        [self.window.delegate broadcastKeyEvent:e];   // ← adapt this!
        // or: carrot::events::broadcast(e); if you have a global multicast
    };

    // ─────────────────────────────────────────────
    // Shift
    if (changed & NSEventModifierFlagShift) {
        bool isDownNow = (current & NSEventModifierFlagShift) != 0;
        // Unfortunately no left/right distinction here — send both or pick one
        // Many games just send a generic Shift press/release
        sendKeyEvent(carrot::input::key_code::left_shift, isDownNow ? carrot::events::key_action::press : carrot::events::key_action::release);
        // If you really need left/right, see "Advanced" section below
    }

    // ─────────────────────────────────────────────
    // Control
    if (changed & NSEventModifierFlagControl) {
        bool isDownNow = (current & NSEventModifierFlagControl) != 0;
        sendKeyEvent(carrot::input::key_code::left_control, isDownNow ? carrot::events::key_action::press : carrot::events::key_action::release);
    }

    // ─────────────────────────────────────────────
    // Option (Alt)
    if (changed & NSEventModifierFlagOption) {
        bool isDownNow = (current & NSEventModifierFlagOption) != 0;
        sendKeyEvent(carrot::input::key_code::left_alt, isDownNow ? carrot::events::key_action::press : carrot::events::key_action::release);
    }

    // ─────────────────────────────────────────────
    // Command (Super)
    if (changed & NSEventModifierFlagCommand) {
        bool isDownNow = (current & NSEventModifierFlagCommand) != 0;
        sendKeyEvent(carrot::input::key_code::left_super, isDownNow ? carrot::events::key_action::press : carrot::events::key_action::release);
    }

    _previous_modifier_flags = current;

    // Important: usually call super so other responders can see it too
    [super flagsChanged:event];
}

// Mouse buttons
- (void)mouseDown:     (NSEvent *)event { /* forward or handle */ }
- (void)mouseUp:       (NSEvent *)event { /* ... */ }
- (void)rightMouseDown:(NSEvent *)event { /* ... */ }
- (void)rightMouseUp:  (NSEvent *)event { /* ... */ }
- (void)otherMouseDown:(NSEvent *)event { /* ... */ }
- (void)otherMouseUp:  (NSEvent *)event { /* ... */ }

// Dragging (while button down)
- (void)mouseDragged:     (NSEvent *)event { /* ... */ }
- (void)rightMouseDragged:(NSEvent *)event { /* ... */ }

// Scroll (trackpad / mouse wheel)
- (void)scrollWheel:(NSEvent *)event
{
    // deltaX / deltaY / deltaZ — usually you want scrollingY
}

// Raw-ish mouse movement
- (void)mouseMoved:(NSEvent *)event
{
    // Will arrive only after setAcceptsMouseMovedEvents:YES
}

@end
