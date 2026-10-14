// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <vector>

struct InputData {
    int width = 0;
    int height = 0;
    std::vector<float> pixels; // RGBA, float32 uncapped
};
