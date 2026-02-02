//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <AppKit/AppKit.h>

@interface WindowView : NSView
{
    NSEventModifierFlags _previous_modifier_flags;
}

- (instancetype)initWithFrame:(NSRect)frameRect NS_DESIGNATED_INITIALIZER;

@end