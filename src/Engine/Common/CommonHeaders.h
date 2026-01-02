//
// Created by zshrout on 12/29/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Core/Logger.h"
#include "Utils/Assert.h"

#include <cstdint>

#ifndef DISABLE_COPY
#define DISABLE_COPY(T)         \
explicit T(const T&) = delete;  \
T& operator=(const T&) = delete;
#endif

#ifndef DISABLE_MOVE
#define DISABLE_MOVE(T)     \
explicit T(T&&) = delete;   \
T& operator=(T&&) = delete;
#endif

#ifndef DISABLE_COPY_AND_MOVE
#define DISABLE_COPY_AND_MOVE(T) DISABLE_COPY(T) DISABLE_MOVE(T)
#endif

#ifdef _DEBUG
#define DEBUG_OP(x) x
#else
#define DEBUG_OP(x)
#endif