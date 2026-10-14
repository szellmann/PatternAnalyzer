// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#include "TestDataGenerator.h"
#include <cmath>
#include <algorithm>
#include <random>

InputData TestDataGenerator::createSyntheticPattern(int width, int height, bool noisy, float noiseIntensity) {
    InputData img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height * 4);

    std::mt19937 rng(noisy ? 1337 : 42);
    std::uniform_real_distribution<float> noiseDist(-noiseIntensity, noiseIntensity);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;

            float u = (float)x / (width - 1);
            float v = (float)y / (height - 1);

            float r = u;
            float g = v;
            float b = std::sin(u * 6.283f) * 0.5f + 0.5f;

            float cx = x - width * 0.5f;
            float cy = y - height * 0.5f;
            float dist = std::sqrt(cx * cx + cy * cy);
            if (std::fmod(dist, 40.0f) < 20.0f) {
                r = std::min(1.0f, r + 50.0f / 255.0f);
                b = std::max(0.0f, b - 50.0f / 255.0f);
            }

            if (noisy) {
                r = std::clamp(r + noiseDist(rng), 0.0f, 1.0f);
                g = std::clamp(g + noiseDist(rng), 0.0f, 1.0f);
                b = std::clamp(b + noiseDist(rng), 0.0f, 1.0f);
            }

            img.pixels[idx + 0] = r;
            img.pixels[idx + 1] = g;
            img.pixels[idx + 2] = b;
            img.pixels[idx + 3] = 1.0f;
        }
    }
    return img;
}

void TestDataGenerator::loadPresetDefault(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    img1 = createSyntheticPattern(512, 512, false, 0.1f);
    img2 = createSyntheticPattern(512, 512, true, 0.1f);
    path1 = "(Synthetic Clean)";
    path2 = "(Synthetic Noisy)";
}

void TestDataGenerator::loadPresetIdentical(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    img1 = createSyntheticPattern(512, 512, false, 0.1f);
    img2 = img1;
    path1 = "(Synthetic Identical 1)";
    path2 = "(Synthetic Identical 2)";
}

void TestDataGenerator::loadPresetBlackWhite(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    int w = 512, h = 512;
    img1.width = w; img1.height = h;
    img1.pixels.assign(w * h * 4, 0.0f);
    for (int i = 3; i < w * h * 4; i += 4) img1.pixels[i] = 1.0f;

    img2.width = w; img2.height = h;
    img2.pixels.assign(w * h * 4, 1.0f);
    path1 = "(Preset Black)";
    path2 = "(Preset White)";
}

void TestDataGenerator::loadPresetClustered(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    int w = 512, h = 512;
    img1.width = w; img1.height = h;
    img1.pixels.assign(w * h * 4, 0.5f);
    for (int i = 3; i < w * h * 4; i += 4) img1.pixels[i] = 1.0f;

    img2 = img1;
    for (int y = 200; y < 300; ++y) {
        for (int x = 200; x < 300; ++x) {
            int idx = (y * w + x) * 4;
            img2.pixels[idx + 0] = (float)((x + y) % 255) / 255.0f;
            img2.pixels[idx + 1] = (float)((x * y) % 255) / 255.0f;
            img2.pixels[idx + 2] = 1.0f - (float)((x + y) % 255) / 255.0f;
        }
    }
    path1 = "(Preset Base Uniform)";
    path2 = "(Preset Clustered Stress-Test)";
}

void TestDataGenerator::loadPresetCheckerboard(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    int w = 512, h = 512;
    int blockSize1 = 37;
    int blockSize2 = 23;

    img1.width = w; img1.height = h;
    img1.pixels.resize(w * h * 4);
    img2.width = w; img2.height = h;
    img2.pixels.resize(w * h * 4);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = (y * w + x) * 4;
            
            bool block1 = ((x / blockSize1) + (y / blockSize1)) % 2 == 0;
            float val1 = block1 ? 0.12f : 0.86f;
            img1.pixels[idx + 0] = val1;
            img1.pixels[idx + 1] = val1;
            img1.pixels[idx + 2] = val1;
            img1.pixels[idx + 3] = 1.0f;

            bool block2 = ((x / blockSize2) + (y / blockSize2)) % 2 == 0;
            float val2 = block2 ? 0.20f : 0.78f;
            img2.pixels[idx + 0] = val2;
            img2.pixels[idx + 1] = val2;
            img2.pixels[idx + 2] = val2;
            img2.pixels[idx + 3] = 1.0f;
        }
    }
    path1 = "(Preset Checkerboard 37)";
    path2 = "(Preset Checkerboard 23)";
}

void TestDataGenerator::loadPresetGradientEdge(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    int w = 512, h = 512;
    img1.width = w; img1.height = h;
    img1.pixels.resize(w * h * 4);
    img2.width = w; img2.height = h;
    img2.pixels.resize(w * h * 4);

    float cx = w * 0.5f;
    float cy = h * 0.5f;

    float theta1 = 0.0f;
    float theta2 = 0.523599f; // ~30 degrees rotation

    auto renderImage = [&](InputData& img, float theta) {
        float cos_t = std::cos(theta);
        float sin_t = std::sin(theta);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = (y * w + x) * 4;
                float rx = (float)x - cx;
                float ry = (float)y - cy;
                float rx_rot = rx * cos_t + ry * sin_t;
                
                float grad = std::clamp((rx_rot + cx) / (float)w, 0.0f, 1.0f);
                if (std::abs(rx_rot) < 15.0f) grad = 1.0f;

                img.pixels[idx + 0] = grad;
                img.pixels[idx + 1] = grad;
                img.pixels[idx + 2] = grad;
                img.pixels[idx + 3] = 1.0f;
            }
        }
    };

    renderImage(img1, theta1);
    renderImage(img2, theta2);
    path1 = "(Preset Gradient 0 deg)";
    path2 = "(Preset Gradient 30 deg)";
}

void TestDataGenerator::loadPresetShiftedGradient(InputData& img1, InputData& img2, std::string& path1, std::string& path2) {
    int w = 512, h = 512;
    img1.width = w; img1.height = h;
    img1.pixels.resize(w * h * 4);
    img2.width = w; img2.height = h;
    img2.pixels.resize(w * h * 4);

    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float theta = 0.523599f;
    float shiftX1 = -40.0f;
    float shiftX2 = 100.0f;

    auto renderImage = [&](InputData& img, float theta_val, float offsetX) {
        float cos_t = std::cos(theta_val);
        float sin_t = std::sin(theta_val);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = (y * w + x) * 4;
                float rx = (float)x - cx - offsetX;
                float ry = (float)y - cy;
                float rx_rot = rx * cos_t + ry * sin_t;
                
                float grad = std::clamp((rx_rot + cx) / (float)w, 0.0f, 1.0f);
                float r, g, b;

                if (std::abs(rx_rot) < 15.0f) {
                    float h_val = (float)y / (float)h * 360.0f;
                    float c = 1.0f;
                    float x_val = c * (1.0f - std::abs(std::fmod(h_val / 60.0f, 2.0f) - 1.0f));
                    float r1=0, g1=0, b1=0;
                    if (h_val < 60) { r1=c; g1=x_val; b1=0; }
                    else if (h_val < 120) { r1=x_val; g1=c; b1=0; }
                    else if (h_val < 180) { r1=0; g1=c; b1=x_val; }
                    else if (h_val < 240) { r1=0; g1=x_val; b1=c; }
                    else if (h_val < 300) { r1=x_val; g1=0; b1=c; }
                    else { r1=c; g1=0; b1=x_val; }
                    r = r1; g = g1; b = b1;
                } else {
                    r = grad; g = grad; b = grad;
                }

                img.pixels[idx + 0] = r;
                img.pixels[idx + 1] = g;
                img.pixels[idx + 2] = b;
                img.pixels[idx + 3] = 1.0f;
            }
        }
    };

    renderImage(img1, theta, shiftX1);
    renderImage(img2, theta, shiftX2);
    path1 = "(Preset Shifted -40px)";
    path2 = "(Preset Shifted +100px)";
}
