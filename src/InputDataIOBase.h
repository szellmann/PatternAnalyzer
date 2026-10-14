// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <string>
#include "InputDataTypes.h"

class InputDataIOBase {
public:
    virtual ~InputDataIOBase() = default;
    virtual InputData load(const std::string& path) = 0;
};
