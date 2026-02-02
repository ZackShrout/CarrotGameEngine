//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppDelegate.h"

#include "WindowView.h"

@implementation app_delegate_t

@synthesize window = _window;
@synthesize info = _info;

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info
{
    self = [super init];
    if (self) {
        _info = info;
    }
    return self;
}

- (void)dealloc
{
    [_window release];
    [super dealloc];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    NSApplication *app = [notification object];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    //NSApplication *app = [notification object];
    //[app activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
    // Called when user clicks red button (or code calls [window close])
    // Since this is (likely) the only/main window, terminate the app
    [NSApp terminate:nil];
}

- (NSWindow *)createAndReturnWindow
{
    _window = [[NSWindow alloc] initWithContentRect:_info._window_rect
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];

    [_window setTitle:[NSString stringWithUTF8String:_info._window_title.data()]];
    [_window center];

    // Very important for games / keyboard input
    [_window setAcceptsMouseMovedEvents:YES];
    [_window setRestorable:NO];           // usually good default for games

    [_window setDelegate:self];

    // Make it key & front
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    WindowView *view = [[WindowView alloc] initWithFrame:[_window contentRectForFrameRect:[_window frame]]];
    [_window setContentView:view];
    [view release];

    [view becomeFirstResponder]; // or [_window makeFirstResponder:view];

    return _window;
}

@end