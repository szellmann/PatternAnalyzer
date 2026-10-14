// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "InputDataTypes.h"
#include <string>

class TestDataGenerator {
public:
    static InputData createSyntheticPattern(int width, int height, bool shifted = false, float shiftAmount = 0.0f);
    
    static void loadPresetDefault(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetIdentical(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetBlackWhite(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetClustered(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetCheckerboard(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetGradientEdge(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
    static void loadPresetShiftedGradient(InputData& img1, InputData& img2, std::string& path1, std::string& path2);
};
