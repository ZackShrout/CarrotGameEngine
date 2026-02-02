//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WindowView.h"

@implementation WindowView

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

// Optional but recommended
- (void)flagsChanged:(NSEvent *)event
{
    // modifier keys only (shift/ctrl/alt/cmd/caps)
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
