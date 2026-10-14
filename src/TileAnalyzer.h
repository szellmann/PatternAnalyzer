// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "InputDataTypes.h"

struct TileInfo {
    int x, y;
    float score;
};

class TileAnalyzer {
public:
    static std::vector<TileInfo> computeTopDifferences(const InputData& img1, const InputData& img2, int tileSize, int topN, int metricType = 0) {
        std::vector<TileInfo> allTiles;
        int cols = (img1.width + tileSize - 1) / tileSize;
        int rows = (img1.height + tileSize - 1) / tileSize;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int tx = c * tileSize;
                int ty = r * tileSize;

                float accum = 0.0f;
                float maxDiff = 0.0f;
                int count = 0;

                int maxX = std::min(tx + tileSize, img1.width);
                int maxY = std::min(ty + tileSize, img1.height);

                for (int y = ty; y < maxY; ++y) {
                    for (int x = tx; x < maxX; ++x) {
                        int idx = (y * img1.width + x) * 4;
                        float r1 = img1.pixels[idx + 0];
                        float g1 = img1.pixels[idx + 1];
                        float b1 = img1.pixels[idx + 2];

                        int x2 = std::clamp(x, 0, img2.width - 1);
                        int y2 = std::clamp(y, 0, img2.height - 1);
                        int idx2 = (y2 * img2.width + x2) * 4;

                        float r2 = img2.pixels[idx2 + 0];
                        float g2 = img2.pixels[idx2 + 1];
                        float b2 = img2.pixels[idx2 + 2];

                        float dr = r1 - r2;
                        float dg = g1 - g2;
                        float db = b1 - b2;

                        if (metricType == 0) { // MSE
                            accum += (dr * dr + dg * dg + db * db) / 3.0f;
                        } else if (metricType == 1) { // MAE
                            accum += (std::abs(dr) + std::abs(dg) + std::abs(db)) / 3.0f;
                        } else if (metricType == 2) { // Max Diff
                            float diff = std::max({std::abs(dr), std::abs(dg), std::abs(db)});
                            maxDiff = std::max(maxDiff, diff);
                        }
                        count++;
                    }
                }

                float score = 0.0f;
                if (count > 0) {
                    score = (metricType == 2) ? maxDiff : (accum / count);
                }
                allTiles.push_back({tx, ty, score});
            }
        }

        std::sort(allTiles.begin(), allTiles.end(), [](const TileInfo& a, const TileInfo& b) {
            return a.score > b.score;
        });

        if ((int)allTiles.size() > topN) {
            allTiles.resize(topN);
        }
        return allTiles;
    }
};
