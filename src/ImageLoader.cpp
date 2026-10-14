// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#include "ImageLoader.h"
#include <algorithm>
#include <cmath>
#include <regex>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

InputData ImageLoader::load(const std::string& path) {
    return loadFromFile(path);
}

InputData ImageLoader::loadFromFile(const std::string& path) {
    InputData img;
    int width, height, channels;
    
    std::string ext = "";
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        ext = path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    bool isHdrExr = (ext == ".hdr" || ext == ".exr");
    bool isRaw = (ext == ".raw");

    if (isRaw) {
        // Parse dimensions from filename: e.g., QC_10000x2000x1_float32.raw
        std::regex dimRegex("_([0-9]+)x([0-9]+)x([0-9]+)_");
        std::smatch match;
        int width = 0, height = 0, depth = 1;

        if (std::regex_search(path, match, dimRegex) && match.size() >= 4) {
            width = std::stoi(match[1].str());
            height = std::stoi(match[2].str());
            depth = std::stoi(match[3].str());
        } else {
            width = 512;
            height = 512;
        }

        std::ifstream file(path, std::ios::binary);
        if (file.is_open() && width > 0 && height > 0) {
            size_t sliceSize = (size_t)width * height;
            std::vector<float> rawData(sliceSize);
            file.read(reinterpret_cast<char*>(rawData.data()), sliceSize * sizeof(float));
            file.close();

            float minVal = std::numeric_limits<float>::max();
            float maxVal = std::numeric_limits<float>::lowest();
            for (float val : rawData) {
                if (!std::isnan(val) && !std::isinf(val)) {
                    minVal = std::min(minVal, val);
                    maxVal = std::max(maxVal, val);
                }
            }
            if (maxVal - minVal < 1e-5f) {
                maxVal = minVal + 1.0f;
            }

            img.width = width;
            img.height = height;
            img.pixels.resize(sliceSize * 4);

            for (int y = 0; y < height; ++y) {
                int srcY = height - 1 - y; // Flip vertically
                for (int x = 0; x < width; ++x) {
                    size_t srcIdx = (size_t)srcY * width + x;
                    float val = rawData[srcIdx];

#if 0
                    float normalized = (val - minVal) / (maxVal - minVal);
                    normalized = std::clamp(normalized, 0.0f, 1.0f);

                    size_t dstIdx = ((size_t)y * width + x) * 4;
                    img.pixels[dstIdx + 0] = normalized; // R
                    img.pixels[dstIdx + 1] = normalized; // G
                    img.pixels[dstIdx + 2] = normalized; // B
                    img.pixels[dstIdx + 3] = 1.0f;       // Alpha = 1.0
#endif

                    size_t dstIdx = ((size_t)y * width + x) * 4;
                    if (val > 1e-10f) {
                    img.pixels[dstIdx + 0] = 0.0f;
                    img.pixels[dstIdx + 1] = 0.2f;
                    img.pixels[dstIdx + 2] = 1.0f;
                    img.pixels[dstIdx + 3] = 1.0f;
                    } else {
                    img.pixels[dstIdx + 0] = 1.0f;
                    img.pixels[dstIdx + 1] = 1.0f;
                    img.pixels[dstIdx + 2] = 1.0f;
                    img.pixels[dstIdx + 3] = 1.0f;
                    }
                }
            }
        }
    } else if (isHdrExr) {
        float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
        if (data) {
            img.width = width;
            img.height = height;
            img.pixels.resize(width * height * 4);
            for (int i = 0; i < width * height * 4; ++i) {
                img.pixels[i] = data[i];
            }
            if (channels < 4) {
                for (int i = 0; i < width * height; ++i) {
                    img.pixels[i * 4 + 3] = 1.0f; // Default alpha
                }
            }
            stbi_image_free(data);
        }
    } else {
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (data) {
            img.width = width;
            img.height = height;
            img.pixels.resize(width * height * 4);
            for (int i = 0; i < width * height * 4; ++i) {
                float val = (float)data[i] / 255.0f; // Normalize LDR [0..255] to [0..1]
                img.pixels[i] = val;
            }
            if (channels < 4) {
                for (int i = 0; i < width * height; ++i) {
                    img.pixels[i * 4 + 3] = 1.0f;
                }
            }
            stbi_image_free(data);
        }
    }
    return img;
}
