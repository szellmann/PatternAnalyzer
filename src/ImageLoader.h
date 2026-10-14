// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "InputDataIOBase.h"
#include <string>

class ImageLoader : public InputDataIOBase {
public:
    InputData load(const std::string& path) override;
    static InputData loadFromFile(const std::string& path);
};
