//
// Created by Zack Shrout on 3/23/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "CocoaContentView.h"

#import "CocoaWindow.h"

@interface cocoa_content_view_t ()
{
    NSTrackingArea* _tracking_area;
    carrot::core::platform::cocoa_window_t* _owner;
    BOOL _mouse_inside;
}
@end

@implementation cocoa_content_view_t

- (instancetype)initWithFrame:(NSRect)frame owner:(carrot::core::platform::cocoa_window_t*)owner
{
    self = [super initWithFrame:frame];
    if (self)
    {
        _owner = owner;
        _tracking_area = nil;
        _mouse_inside = NO;

        [self setWantsLayer:YES];
    }
    return self;
}

- (void)dealloc
{
    if (_tracking_area)
    {
        [self removeTrackingArea:_tracking_area];
        [_tracking_area release];
        _tracking_area = nil;
    }

    [super dealloc];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];

    if (_tracking_area)
    {
        [self removeTrackingArea:_tracking_area];
        [_tracking_area release];
        _tracking_area = nil;
    }

    NSTrackingAreaOptions options =
        NSTrackingMouseEnteredAndExited |
        NSTrackingMouseMoved |
        NSTrackingActiveInKeyWindow |
        NSTrackingInVisibleRect;

    _tracking_area = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                 options:options
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_tracking_area];
}

- (void)mouseEntered:(NSEvent *)event
{
    _mouse_inside = YES;
    if (_owner) _owner->handle_mouse_entered();
}

- (void)mouseExited:(NSEvent *)event
{
    _mouse_inside = NO;
    if (_owner) _owner->handle_mouse_exited();
}

- (void)mouseMoved:(NSEvent *)event
{
    if (!_mouse_inside) return;
    if (_owner) _owner->handle_mouse_moved(event, self);
}

- (void)mouseDown:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, true);
}

- (void)mouseUp:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, false);
}

- (void)rightMouseDown:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, true);
}

- (void)rightMouseUp:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, false);
}

- (void)otherMouseDown:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, true);
}

- (void)otherMouseUp:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_button(event, self, false);
}

- (void)mouseDragged:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_dragged(event, self);
}

- (void)rightMouseDragged:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_dragged(event, self);
}

- (void)otherMouseDragged:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_dragged(event, self);
}

- (void)scrollWheel:(NSEvent *)event
{
    if (_owner) _owner->handle_mouse_scrolled(event, self);
}

- (void)keyDown:(NSEvent *)event
{
    if (_owner) _owner->handle_key_event(event, true);
}

- (void)keyUp:(NSEvent *)event
{
    if (_owner) _owner->handle_key_event(event, false);
}

- (BOOL)isFlipped
{
    return NO;
}

@end
