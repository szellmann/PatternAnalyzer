// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#include "TileAnalyzer.h"
#include <cmath>
#include <algorithm>

std::vector<TileInfo> TileAnalyzer::computeTopDifferences(
    const ImageData& img1, const ImageData& img2,
    int tileSize, int topN)
{
    std::vector<TileInfo> tiles;
    if (img1.width <= 0 || img1.height <= 0 || img2.width <= 0 || img2.height <= 0)
        return tiles;

    int cols = (img1.width + tileSize - 1) / tileSize;
    int rows = (img1.height + tileSize - 1) / tileSize;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int imgX = c * tileSize;
            int imgY = r * tileSize;

            double accumMSE = 0.0;
            int pixelCount = 0;

            int step = std::max(1, tileSize / 16); // Sample step for performance
            for (int ty = 0; ty < tileSize && (imgY + ty) < img1.height; ty += step) {
                for (int tx = 0; tx < tileSize && (imgX + tx) < img1.width; tx += step) {
                    int ix = imgX + tx;
                    int iy = imgY + ty;

                    int idx1 = (iy * img1.width + ix) * 4;
                    int idx2 = (iy * img2.width + ix) * 4;

                    if (idx1 + 3 < (int)img1.pixels.size() && idx2 + 3 < (int)img2.pixels.size()) {
                        float dr = (float)img1.pixels[idx1 + 0] - (float)img2.pixels[idx2 + 0];
                        float dg = (float)img1.pixels[idx1 + 1] - (float)img2.pixels[idx2 + 1];
                        float db = (float)img1.pixels[idx1 + 2] - (float)img2.pixels[idx2 + 2];

                        accumMSE += (dr * dr + dg * dg + db * db) / 3.0f;
                        pixelCount++;
                    }
                }
            }

            float mse = (pixelCount > 0) ? (float)(accumMSE / pixelCount) : 0.0f;
            tiles.push_back({imgX, imgY, mse});
        }
    }

    std::sort(tiles.begin(), tiles.end(), [](const TileInfo& a, const TileInfo& b) {
        return a.mse > b.mse;
    });

    if ((int)tiles.size() > topN) {
        tiles.resize(topN);
    }
    return tiles;
}
