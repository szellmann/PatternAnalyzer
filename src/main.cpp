// Copyright 2026-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#define GL_SILENCE_DEPRECATION

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "InputDataTypes.h"
#include "ImageLoader.h"
#include "TestDataGenerator.h"
#include "TileAnalyzer.h"
#include "portable-file-dialogs.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

struct ImageLayer {
    std::string name;
    InputData data;
    GLuint textureID = 0;
    bool visible = true;
    float opacity = 1.0f;
};

static ImVec2 panel1Min(0, 0), panel1Max(0, 0);
static ImVec2 panel2Min(0, 0), panel2Max(0, 0);
static std::string pendingDropPath1 = "";
static std::string pendingDropPath2 = "";

static std::vector<ImageLayer> layers1;
static int activeLayer1 = 0;

static std::vector<ImageLayer> layers2;
static int activeLayer2 = 0;

static std::vector<std::string> recentlyOpenedFiles;
static std::string selectedRecentFile = "";
static bool showRecentChoiceModal = false;
static bool showSettingsModal = false;
static bool showHelpModal = false;

static int minTileSize = 16;
static int maxTileSize = 128;

const std::string iniFilename = "recent_files.ini";
const std::string settingsFilename = "settings.ini";

void loadSettings() {
    std::ifstream file(settingsFilename);
    if (file.is_open()) {
        file >> minTileSize >> maxTileSize;
        file.close();
    } else {
        minTileSize = 16;
        maxTileSize = 128;
    }
    minTileSize = std::clamp(minTileSize, 1, 2048);
    maxTileSize = std::clamp(maxTileSize, minTileSize, 2048);
}

void saveSettings() {
    std::ofstream file(settingsFilename);
    if (file.is_open()) {
        file << minTileSize << " " << maxTileSize << "\n";
        file.close();
    }
}

void loadRecentFiles() {
    recentlyOpenedFiles.clear();
    std::ifstream file(iniFilename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                recentlyOpenedFiles.push_back(line);
            }
        }
        file.close();
    }
}

void saveRecentFiles() {
    std::ofstream file(iniFilename);
    if (file.is_open()) {
        for (const auto& path : recentlyOpenedFiles) {
            file << path << "\n";
        }
        file.close();
    }
}

void addRecentFile(const std::string& path) {
    if (path.empty()) return;
    auto it = std::find(recentlyOpenedFiles.begin(), recentlyOpenedFiles.end(), path);
    if (it != recentlyOpenedFiles.end()) {
        recentlyOpenedFiles.erase(it);
    }
    recentlyOpenedFiles.insert(recentlyOpenedFiles.begin(), path);
    if (recentlyOpenedFiles.size() > 10) {
        recentlyOpenedFiles.pop_back();
    }
    saveRecentFiles();
}

GLuint createTextureFromData(const InputData& img) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (img.width > 0 && img.height > 0 && !img.pixels.empty()) {
        std::vector<unsigned char> tempBytes(img.width * img.height * 4);
        for (size_t i = 0; i < img.pixels.size(); ++i) {
            tempBytes[i] = (unsigned char)(std::clamp<float>(img.pixels[i], 0.0f, 1.0f) * 255.0f);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempBytes.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

void updateTexture(GLuint textureID, const InputData& img) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (img.width > 0 && img.height > 0 && !img.pixels.empty()) {
        std::vector<unsigned char> tempBytes(img.width * img.height * 4);
        for (size_t i = 0; i < img.pixels.size(); ++i) {
            tempBytes[i] = (unsigned char)(std::clamp<float>(img.pixels[i], 0.0f, 1.0f) * 255.0f);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempBytes.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

InputData resizeImage(const InputData& src, int newWidth, int newHeight) {
    InputData dst;
    dst.width = newWidth;
    dst.height = newHeight;
    dst.pixels.resize(newWidth * newHeight * 4);
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = (int)((float)x / newWidth * src.width);
            int srcY = (int)((float)y / newHeight * src.height);
            srcX = std::clamp(srcX, 0, src.width - 1);
            srcY = std::clamp(srcY, 0, src.height - 1);
            
            int srcIdx = (srcY * src.width + srcX) * 4;
            int dstIdx = (y * newWidth + x) * 4;
            
            dst.pixels[dstIdx + 0] = src.pixels[srcIdx + 0];
            dst.pixels[dstIdx + 1] = src.pixels[srcIdx + 1];
            dst.pixels[dstIdx + 2] = src.pixels[srcIdx + 2];
            dst.pixels[dstIdx + 3] = src.pixels[srcIdx + 3];
        }
    }
    return dst;
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {
    if (count > 0 && paths && paths[0]) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        if (mx >= panel1Min.x && mx <= panel1Max.x && my >= panel1Min.y && my <= panel1Max.y) {
            pendingDropPath1 = paths[0];
        } else if (mx >= panel2Min.x && mx <= panel2Max.x && my >= panel2Min.y && my <= panel2Max.y) {
            pendingDropPath2 = paths[0];
        } else {
            pendingDropPath1 = paths[0];
        }
    }
}

float computeTileMetric(const InputData& img1, const InputData& img2, int tx, int ty, int tileSize, int metricType) {
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

            if (metricType == 0) {
                accum += (dr * dr + dg * dg + db * db) / 3.0f;
            } else if (metricType == 1) {
                accum += (std::abs(dr) + std::abs(dg) + std::abs(db)) / 3.0f;
            } else if (metricType == 2) {
                float diff = std::max({std::abs(dr), std::abs(dg), std::abs(db)});
                maxDiff = std::max(maxDiff, diff);
            }
            count++;
        }
    }

    if (count == 0) return 0.0f;
    if (metricType == 2) return maxDiff;
    return accum / count;
}

struct MatchResult {
    float score;
    int bestDx;
    int bestDy;
};

MatchResult computeTileTranslation(const InputData& img1, const InputData& img2, int tx, int ty, int tileSize, int metricType) {
    float bestScore = 1e9f;
    int bestDx = 0;
    int bestDy = 0;
    int searchRadius = 12;
    int searchMetric = (metricType >= 3) ? metricType : 3;

    for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
        for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
            float currentScore = 0.0f;
            int count = 0;

            int maxX = std::min(tx + tileSize, img1.width);
            int maxY = std::min(ty + tileSize, img1.height);

            for (int y = ty; y < maxY; ++y) {
                for (int x = tx; x < maxX; ++x) {
                    int x2 = x + dx;
                    int y2 = y + dy;

                    float r1 = img1.pixels[(y * img1.width + x) * 4 + 0];
                    float g1 = img1.pixels[(y * img1.width + x) * 4 + 1];
                    float b1 = img1.pixels[(y * img1.width + x) * 4 + 2];

                    int cx2 = std::clamp(x2, 0, img2.width - 1);
                    int cy2 = std::clamp(y2, 0, img2.height - 1);
                    int idx2 = (cy2 * img2.width + cx2) * 4;

                    float r2 = img2.pixels[idx2 + 0];
                    float g2 = img2.pixels[idx2 + 1];
                    float b2 = img2.pixels[idx2 + 2];

                    float dr = r1 - r2;
                    float dg = g1 - g2;
                    float db = b1 - b2;

                    if (searchMetric == 4) {
                        currentScore += (std::abs(dr) + std::abs(dg) + std::abs(db));
                    } else {
                        currentScore += (dr * dr + dg * dg + db * db);
                    }
                    count++;
                }
            }
            if (count > 0 && currentScore < bestScore) {
                bestScore = currentScore;
                bestDx = dx;
                bestDy = dy;
            }
        }
    }
    return {bestScore, bestDx, bestDy};
}

struct TranslationTileInfo {
    int x, y;
    float score;
    int dx, dy;
};

std::vector<TranslationTileInfo> computeTopTranslations(const InputData& img1, const InputData& img2, int tileSize, int topN, int metricType) {
    std::vector<TranslationTileInfo> allTiles;
    int cols = (img1.width + tileSize - 1) / tileSize;
    int rows = (img1.height + tileSize - 1) / tileSize;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int tx = c * tileSize;
            int ty = r * tileSize;
            MatchResult res = computeTileTranslation(img1, img2, tx, ty, tileSize, metricType);
            allTiles.push_back({tx, ty, res.score, res.bestDx, res.bestDy});
        }
    }

    std::sort(allTiles.begin(), allTiles.end(), [](const TranslationTileInfo& a, const TranslationTileInfo& b) {
        return a.score > b.score;
    });

    if ((int)allTiles.size() > topN) {
        allTiles.resize(topN);
    }
    return allTiles;
}

std::vector<uint8_t> generateArrowMask(int tileSize, float thicknessMod, float patternMargin, float theta_q) {
    std::vector<uint8_t> mask(tileSize * tileSize, 0);
    float halfThick = std::max(1.0f, (float)tileSize * 0.08f * (1.0f + thicknessMod));
    float cx = tileSize * 0.5f;
    float cy = tileSize * 0.5f;

    float arrowScale = (patternMargin <= 0.0f) ? (1.0f + patternMargin * 0.5f) : (1.0f + patternMargin * 0.25f);
    float cos_t = std::cos(theta_q);
    float sin_t = std::sin(theta_q);

    for (int ty = 0; ty < tileSize; ++ty) {
        for (int tx = 0; tx < tileSize; ++tx) {
            float rx = ((float)tx + 0.5f - cx) / arrowScale;
            float ry = ((float)ty + 0.5f - cy) / arrowScale;

            float rx_rot = rx * cos_t + ry * sin_t;
            float ry_rot = -rx * sin_t + ry * cos_t;

            bool stem = (std::abs(rx_rot) <= halfThick) && (ry_rot >= -0.15f * tileSize && ry_rot <= 0.35f * tileSize);
            float progress = (ry_rot - (-0.45f * tileSize)) / (0.3f * tileSize);
            float maxW = progress * 0.3f * tileSize;
            bool head = (ry_rot >= -0.45f * tileSize && ry_rot <= -0.15f * tileSize && std::abs(rx_rot) <= maxW);

            if (stem || head) {
                mask[ty * tileSize + tx] = 1;
            }
        }
    }
    return mask;
}

ImVec4 getParaViewCoolWarmColor(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    static const float table[33][3] = {
        {59.0f/255.0f,  76.0f/255.0f,  192.0f/255.0f},
        {68.0f/255.0f,  90.0f/255.0f,  204.0f/255.0f},
        {77.0f/255.0f,  104.0f/255.0f, 215.0f/255.0f},
        {87.0f/255.0f,  117.0f/255.0f, 225.0f/255.0f},
        {98.0f/255.0f,  130.0f/255.0f, 234.0f/255.0f},
        {108.0f/255.0f, 142.0f/255.0f, 241.0f/255.0f},
        {119.0f/255.0f, 154.0f/255.0f, 247.0f/255.0f},
        {130.0f/255.0f, 165.0f/255.0f, 251.0f/255.0f},
        {141.0f/255.0f, 176.0f/255.0f, 254.0f/255.0f},
        {152.0f/255.0f, 185.0f/255.0f, 255.0f/255.0f},
        {163.0f/255.0f, 194.0f/255.0f, 255.0f/255.0f},
        {174.0f/255.0f, 201.0f/255.0f, 253.0f/255.0f},
        {184.0f/255.0f, 208.0f/255.0f, 249.0f/255.0f},
        {194.0f/255.0f, 213.0f/255.0f, 244.0f/255.0f},
        {204.0f/255.0f, 217.0f/255.0f, 238.0f/255.0f},
        {213.0f/255.0f, 219.0f/255.0f, 230.0f/255.0f},
        {221.0f/255.0f, 221.0f/255.0f, 221.0f/255.0f},
        {229.0f/255.0f, 213.0f/255.0f, 213.0f/255.0f},
        {234.0f/255.0f, 204.0f/255.0f, 204.0f/255.0f},
        {238.0f/255.0f, 194.0f/255.0f, 194.0f/255.0f},
        {241.0f/255.0f, 182.0f/255.0f, 182.0f/255.0f},
        {242.0f/255.0f, 169.0f/255.0f, 169.0f/255.0f},
        {242.0f/255.0f, 155.0f/255.0f, 155.0f/255.0f},
        {241.0f/255.0f, 140.0f/255.0f, 140.0f/255.0f},
        {239.0f/255.0f, 125.0f/255.0f, 125.0f/255.0f},
        {235.0f/255.0f, 108.0f/255.0f, 108.0f/255.0f},
        {229.0f/255.0f, 91.0f/255.0f,  91.0f/255.0f},
        {221.0f/255.0f, 73.0f/255.0f,  73.0f/255.0f},
        {211.0f/255.0f, 54.0f/255.0f,  54.0f/255.0f},
        {199.0f/255.0f, 37.0f/255.0f,  37.0f/255.0f},
        {185.0f/255.0f, 20.0f/255.0f,  20.0f/255.0f},
        {170.0f/255.0f, 0.0f/255.0f,   0.0f/255.0f},
        {153.0f/255.0f, 0.0f/255.0f,   0.0f/255.0f}
    };
    int n = 33;
    float scaled = t * (n - 1);
    int idx = (int)scaled;
    if (idx >= n - 1) return ImVec4(table[n-1][0], table[n-1][1], table[n-1][2], 1.0f);
    float frac = scaled - idx;
    float r = table[idx][0] + frac * (table[idx+1][0] - table[idx][0]);
    float g = table[idx][1] + frac * (table[idx+1][1] - table[idx][1]);
    float b = table[idx][2] + frac * (table[idx+1][2] - table[idx][2]);
    return ImVec4(r, g, b, 1.0f);
}

void openImage1(bool& cacheValid) {
    ImageLoader loader;
    auto selection = pfd::open_file("Open Image 1", "", 
        { "Input Files", "*.raw *.png *.jpg *.jpeg *.bmp *.tga *.hdr *.exr", "All Files", "*" }).result();
    if (!selection.empty()) {
        InputData loaded = loader.load(selection[0]);
        if (loaded.width > 0 && loaded.height > 0) {
            for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
            layers1.clear();
            ImageLayer l;
            l.name = std::filesystem::path(selection[0]).filename().string();
            l.data = loaded;
            l.textureID = createTextureFromData(loaded);
            l.visible = true;
            l.opacity = 1.0f;
            layers1.push_back(l);
            activeLayer1 = 0;

            if (!layers2.empty() && (layers2[0].data.width != loaded.width || layers2[0].data.height != loaded.height)) {
                for (auto& lyr2 : layers2) {
                    lyr2.data = resizeImage(lyr2.data, loaded.width, loaded.height);
                    updateTexture(lyr2.textureID, lyr2.data);
                }
            }
            addRecentFile(selection[0]);
            cacheValid = false;
        }
    }
}

void addLayer1(bool& cacheValid) {
    ImageLoader loader;
    auto selection = pfd::open_file("Add Layer to Image 1", "", 
        { "Input Files", "*.raw *.png *.jpg *.jpeg *.bmp *.tga *.hdr *.exr", "All Files", "*" }).result();
    if (!selection.empty()) {
        InputData loaded = loader.load(selection[0]);
        if (loaded.width > 0 && loaded.height > 0) {
            int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
            int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
            if (loaded.width != refW || loaded.height != refH) {
                loaded = resizeImage(loaded, refW, refH);
            }
            ImageLayer l;
            l.name = std::filesystem::path(selection[0]).filename().string();
            l.data = loaded;
            l.textureID = createTextureFromData(loaded);
            l.visible = true;
            l.opacity = 1.0f;
            layers1.insert(layers1.begin(), l);
            activeLayer1 = 0;
            addRecentFile(selection[0]);
            cacheValid = false;
        }
    }
}

void openImage2(bool& cacheValid) {
    ImageLoader loader;
    auto selection = pfd::open_file("Open Image 2", "", 
        { "Input Files", "*.raw *.png *.jpg *.jpeg *.bmp *.tga *.hdr *.exr", "All Files", "*" }).result();
    if (!selection.empty()) {
        InputData loaded = loader.load(selection[0]);
        if (loaded.width > 0 && loaded.height > 0) {
            int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
            int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
            if (loaded.width != refW || loaded.height != refH) {
                loaded = resizeImage(loaded, refW, refH);
            }
            for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
            layers2.clear();
            ImageLayer l;
            l.name = std::filesystem::path(selection[0]).filename().string();
            l.data = loaded;
            l.textureID = createTextureFromData(loaded);
            l.visible = true;
            l.opacity = 1.0f;
            layers2.push_back(l);
            activeLayer2 = 0;
            addRecentFile(selection[0]);
            cacheValid = false;
        }
    }
}

void addLayer2(bool& cacheValid) {
    ImageLoader loader;
    auto selection = pfd::open_file("Add Layer to Image 2", "", 
        { "Input Files", "*.raw *.png *.jpg *.jpeg *.bmp *.tga *.hdr *.exr", "All Files", "*" }).result();
    if (!selection.empty()) {
        InputData loaded = loader.load(selection[0]);
        if (loaded.width > 0 && loaded.height > 0) {
            int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
            int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
            if (loaded.width != refW || loaded.height != refH) {
                loaded = resizeImage(loaded, refW, refH);
            }
            ImageLayer l;
            l.name = std::filesystem::path(selection[0]).filename().string();
            l.data = loaded;
            l.textureID = createTextureFromData(loaded);
            l.visible = true;
            l.opacity = 1.0f;
            layers2.insert(layers2.begin(), l);
            activeLayer2 = 0;
            addRecentFile(selection[0]);
            cacheValid = false;
        }
    }
}

int main(int argc, char** argv) {
    loadRecentFiles();
    loadSettings();

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "X marks the spot", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwSetWindowPos(window, 0, 0);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetDropCallback(window, drop_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImageLoader loader;
    InputData preset1, preset2;
    std::string path1, path2;

    if (argc >= 3) {
        InputData loaded1 = loader.load(argv[1]);
        InputData loaded2 = loader.load(argv[2]);

        if (loaded1.width > 0 && loaded1.height > 0) {
            preset1 = loaded1;
            path1 = argv[1];
            addRecentFile(argv[1]);
        } else {
            TestDataGenerator::loadPresetDefault(preset1, preset2, path1, path2);
        }

        if (loaded2.width > 0 && loaded2.height > 0) {
            preset2 = loaded2;
            path2 = argv[2];
            addRecentFile(argv[2]);
        }
    } else {
        TestDataGenerator::loadPresetDefault(preset1, preset2, path1, path2);
    }

    if (preset2.width != preset1.width || preset2.height != preset1.height) {
        preset2 = resizeImage(preset2, preset1.width, preset1.height);
    }

    ImageLayer l1;
    l1.name = std::filesystem::path(path1).filename().string();
    if (l1.name.empty()) l1.name = "(Synthetic Clean)";
    l1.data = preset1;
    l1.textureID = createTextureFromData(preset1);
    layers1.push_back(l1);

    ImageLayer l2;
    l2.name = std::filesystem::path(path2).filename().string();
    if (l2.name.empty()) l2.name = "(Synthetic Noisy)";
    l2.data = preset2;
    l2.textureID = createTextureFromData(preset2);
    layers2.push_back(l2);

    bool smoothImage1 = true;
    bool smoothImage2 = true;

    GLuint tex1Display = createTextureFromData(layers1[0].data);

    float img1Opacity = 1.0f;
    float img2Opacity = 1.0f;
    int tileSize = std::clamp(32, minTileSize, maxTileSize);
    int topN = 10;
    bool allOverlays = false;
    bool showTileGrid = false;
    static ImVec4 gridColor = ImVec4(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 40.0f / 255.0f);
    int patternType = 0;
    int metricType = 0;
    float thicknessMod = 0.0f;
    float patternMargin = 0.0f;

    int patternColorMode = 0;
    ImVec4 solidColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    bool image1Only = false;
    bool image2Only = false;

    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    int zoomIndex = 0;
    int alignmentIndex = 0;
    bool initialFitDone = false;

    bool isSelecting = false;
    ImVec2 selectStartPos(0, 0);
    ImVec2 selectEndPos(0, 0);

    bool cacheValid = false;
    int cachedTileSize = -1;
    int cachedTopN = -1;
    int cachedMetricType = -1;
    int cachedPatternType = -1;
    float cachedThicknessMod = -999.0f;
    float cachedPatternMargin = -999.0f;
    std::vector<TranslationTileInfo> cachedTopTiles;
    std::vector<uint8_t> cachedPatternMask;
    std::vector<std::vector<uint8_t>> cachedArrowMasks(16);
    InputData img1Render;

    bool firstFrame = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (!pendingDropPath1.empty()) {
            InputData loaded = loader.load(pendingDropPath1);
            if (loaded.width > 0 && loaded.height > 0) {
                for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                layers1.clear();
                ImageLayer l;
                l.name = std::filesystem::path(pendingDropPath1).filename().string();
                l.data = loaded;
                l.textureID = createTextureFromData(loaded);
                layers1.push_back(l);
                activeLayer1 = 0;
                updateTexture(tex1Display, loaded);
                addRecentFile(pendingDropPath1);
                cacheValid = false;
            }
            pendingDropPath1.clear();
        }

        if (!pendingDropPath2.empty()) {
            InputData loaded = loader.load(pendingDropPath2);
            if (loaded.width > 0 && loaded.height > 0) {
                int refW = layers1[0].data.width;
                int refH = layers1[0].data.height;
                if (loaded.width != refW || loaded.height != refH) {
                    loaded = resizeImage(loaded, refW, refH);
                }
                for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                layers2.clear();
                ImageLayer l;
                l.name = std::filesystem::path(pendingDropPath2).filename().string();
                l.data = loaded;
                l.textureID = createTextureFromData(loaded);
                layers2.push_back(l);
                activeLayer2 = 0;
                addRecentFile(pendingDropPath2);
                cacheValid = false;
            }
            pendingDropPath2.clear();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            float zoomOld = zoom;
            zoom = std::clamp(zoom * 1.15f, 0.1f, 10.0f);
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            panX = center.x - (center.x - panX) * (zoom / zoomOld);
            panY = center.y - (center.y - panY) * (zoom / zoomOld);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            float zoomOld = zoom;
            zoom = std::clamp(zoom / 1.15f, 0.1f, 10.0f);
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            panX = center.x - (center.x - panX) * (zoom / zoomOld);
            panY = center.y - (center.y - panY) * (zoom / zoomOld);
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Image 1...", "Ctrl+1")) {
                    openImage1(cacheValid);
                }
                if (ImGui::MenuItem("Add Layer to Image 1...")) {
                    addLayer1(cacheValid);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Open Image 2...", "Ctrl+2")) {
                    openImage2(cacheValid);
                }
                if (ImGui::MenuItem("Add Layer to Image 2...")) {
                    addLayer2(cacheValid);
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Recently Opened")) {
                    if (recentlyOpenedFiles.empty()) {
                        ImGui::MenuItem("No recent files", nullptr, false, false);
                    } else {
                        for (const auto& path : recentlyOpenedFiles) {
                            if (ImGui::MenuItem(path.c_str())) {
                                selectedRecentFile = path;
                                showRecentChoiceModal = true;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Settings...")) {
                    showSettingsModal = true;
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Load Test Preset")) {
                    if (ImGui::MenuItem("Default Synthetic Clean vs Noisy")) {
                        TestDataGenerator::loadPresetDefault(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Synthetic Clean)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Synthetic Noisy)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("Identical Image Pair")) {
                        TestDataGenerator::loadPresetIdentical(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Synthetic Identical 1)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Synthetic Identical 2)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("All Black vs. All White")) {
                        TestDataGenerator::loadPresetBlackWhite(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Preset Black)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Preset White)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("Clustered Stress-Test")) {
                        TestDataGenerator::loadPresetClustered(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Preset Base Uniform)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Preset Clustered)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("Checkerboard Pattern")) {
                        TestDataGenerator::loadPresetCheckerboard(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Preset Checkerboard 37)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Preset Checkerboard 23)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("Rotated Edge Gradient")) {
                        TestDataGenerator::loadPresetGradientEdge(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Preset Gradient 0 deg)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Preset Gradient 30 deg)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    if (ImGui::MenuItem("Shifted Gradient")) {
                        TestDataGenerator::loadPresetShiftedGradient(preset1, preset2, path1, path2);
                        for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                        for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                        layers1.clear(); layers2.clear();
                        ImageLayer l1; l1.name = "(Preset Shifted -40px)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
                        ImageLayer l2; l2.name = "(Preset Shifted +100px)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
                        activeLayer1 = 0; activeLayer2 = 0;
                        updateTexture(tex1Display, layers1[0].data);
                        cacheValid = false;
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Keyboard & Mouse Controls...")) {
                    showHelpModal = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (showRecentChoiceModal) ImGui::OpenPopup("Load Recent File As");
        if (ImGui::BeginPopupModal("Load Recent File As", &showRecentChoiceModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Choose target slot or layer for file:\n%s", selectedRecentFile.c_str());
            ImGui::Separator();
            if (ImGui::Button("Open as Image 1", ImVec2(150, 0))) {
                InputData loaded = loader.load(selectedRecentFile);
                if (loaded.width > 0 && loaded.height > 0) {
                    for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
                    layers1.clear();
                    ImageLayer l;
                    l.name = std::filesystem::path(selectedRecentFile).filename().string();
                    l.data = loaded;
                    l.textureID = createTextureFromData(loaded);
                    layers1.push_back(l);
                    activeLayer1 = 0;
                    cacheValid = false;
                }
                showRecentChoiceModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Layer to Image 1", ImVec2(150, 0))) {
                InputData loaded = loader.load(selectedRecentFile);
                if (loaded.width > 0 && loaded.height > 0) {
                    int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
                    int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
                    if (loaded.width != refW || loaded.height != refH) loaded = resizeImage(loaded, refW, refH);
                    ImageLayer l;
                    l.name = std::filesystem::path(selectedRecentFile).filename().string();
                    l.data = loaded;
                    l.textureID = createTextureFromData(loaded);
                    layers1.insert(layers1.begin(), l);
                    activeLayer1 = 0;
                    cacheValid = false;
                }
                showRecentChoiceModal = false;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button("Open as Image 2", ImVec2(150, 0))) {
                InputData loaded = loader.load(selectedRecentFile);
                if (loaded.width > 0 && loaded.height > 0) {
                    int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
                    int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
                    if (loaded.width != refW || loaded.height != refH) loaded = resizeImage(loaded, refW, refH);
                    for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
                    layers2.clear();
                    ImageLayer l;
                    l.name = std::filesystem::path(selectedRecentFile).filename().string();
                    l.data = loaded;
                    l.textureID = createTextureFromData(loaded);
                    layers2.push_back(l);
                    activeLayer2 = 0;
                    cacheValid = false;
                }
                showRecentChoiceModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Layer to Image 2", ImVec2(150, 0))) {
                InputData loaded = loader.load(selectedRecentFile);
                if (loaded.width > 0 && loaded.height > 0) {
                    int refW = layers1.empty() ? loaded.width : layers1[0].data.width;
                    int refH = layers1.empty() ? loaded.height : layers1[0].data.height;
                    if (loaded.width != refW || loaded.height != refH) loaded = resizeImage(loaded, refW, refH);
                    ImageLayer l;
                    l.name = std::filesystem::path(selectedRecentFile).filename().string();
                    l.data = loaded;
                    l.textureID = createTextureFromData(loaded);
                    layers2.insert(layers2.begin(), l);
                    activeLayer2 = 0;
                    cacheValid = false;
                }
                showRecentChoiceModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Spacing();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                showRecentChoiceModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showHelpModal) ImGui::OpenPopup("Keyboard & Mouse Controls");
        if (ImGui::BeginPopupModal("Keyboard & Mouse Controls", &showHelpModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Mouse Navigation (Pattern View)");
            ImGui::Separator();
            ImGui::BulletText("Left-Click Drag / Right-Click Drag: Pan view");
            ImGui::BulletText("Mouse Scroll Wheel: Zoom in / out under cursor");
            ImGui::BulletText("Ctrl + Left-Click Drag: Draw selection box to zoom to region");
            ImGui::BulletText("Hover over Tile: Inspect Tile ID and Difference Metric");
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                showHelpModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showSettingsModal) ImGui::OpenPopup("Preferences / Settings");
        if (ImGui::BeginPopupModal("Preferences / Settings", &showSettingsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Configure Tile Size Range (1 - 2048):");
            ImGui::Separator();
            static int tempMin = minTileSize;
            static int tempMax = maxTileSize;
            ImGui::InputInt("Min Tile Size", &tempMin, 1, 10);
            ImGui::InputInt("Max Tile Size", &tempMax, 1, 10);
            tempMin = std::clamp(tempMin, 1, 2048);
            tempMax = std::clamp(tempMax, 1, 2048);
            if (tempMin > tempMax) tempMax = tempMin;
            ImGui::Spacing();
            if (ImGui::Button("Save & Apply", ImVec2(120, 0))) {
                minTileSize = tempMin;
                maxTileSize = tempMax;
                saveSettings();
                tileSize = std::clamp(tileSize, minTileSize, maxTileSize);
                cacheValid = false;
                showSettingsModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                showSettingsModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Dedicated "Layers" Widget ---
        ImGui::Begin("Layers");
        {
            // Image 1 Layers
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Image 1 Layers (Top = Active)");
            activeLayer1 = std::clamp(activeLayer1, 0, (int)layers1.size() - 1);
            for (int i = 0; i < (int)layers1.size(); ++i) {
                ImGui::PushID(i + 1000);
                bool isSelected = (i == activeLayer1);
                
                // Layer Bar
                ImVec2 barPos = ImGui::GetCursorScreenPos();
                ImVec2 barSize(ImGui::GetContentRegionAvail().x, 26.0f);
                
                ImU32 barCol = isSelected ? IM_COL32(60, 110, 180, 220) : IM_COL32(50, 50, 55, 200);
                ImGui::GetWindowDrawList()->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), barCol, 4.0f);
                ImGui::GetWindowDrawList()->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), IM_COL32(90, 90, 100, 150), 4.0f);

                ImGui::SetCursorScreenPos(ImVec2(barPos.x + 8, barPos.y + 3));
                ImGui::Text("%s", layers1[i].name.c_str());

                // Visibility checkbox and delete on the right
                float rightWidgetsWidth = 60.0f;
                ImGui::SameLine(barSize.x - rightWidgetsWidth);
                ImGui::Checkbox("##vis1", &layers1[i].visible);
                if (layers1.size() > 1) {
                    ImGui::SameLine();
                    if (ImGui::Button("X##del1")) {
                        glDeleteTextures(1, &layers1[i].textureID);
                        layers1.erase(layers1.begin() + i);
                        activeLayer1 = std::clamp(activeLayer1, 0, (int)layers1.size() - 1);
                        cacheValid = false;
                        ImGui::PopID();
                        break;
                    }
                }

                ImGui::SetCursorScreenPos(barPos);
                ImGui::InvisibleButton("layer_bar1", barSize);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    activeLayer1 = i;
                    cacheValid = false;
                }

                // Drag and drop reordering
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
                    ImGui::SetDragDropPayload("DND_LAYER_1", &i, sizeof(int));
                    ImGui::Text("Moving layer: %s", layers1[i].name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_LAYER_1")) {
                        int srcIdx = *(const int*)payload->Data;
                        if (srcIdx != i) {
                            ImageLayer moved = layers1[srcIdx];
                            layers1.erase(layers1.begin() + srcIdx);
                            layers1.insert(layers1.begin() + i, moved);
                            activeLayer1 = i;
                            cacheValid = false;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Image 2 Layers
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.7f, 1.0f), "Image 2 Layers (Top = Active)");
            activeLayer2 = std::clamp(activeLayer2, 0, (int)layers2.size() - 1);
            for (int i = 0; i < (int)layers2.size(); ++i) {
                ImGui::PushID(i + 2000);
                bool isSelected = (i == activeLayer2);
                
                ImVec2 barPos = ImGui::GetCursorScreenPos();
                ImVec2 barSize(ImGui::GetContentRegionAvail().x, 26.0f);
                
                ImU32 barCol = isSelected ? IM_COL32(180, 110, 60, 220) : IM_COL32(50, 50, 55, 200);
                ImGui::GetWindowDrawList()->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), barCol, 4.0f);
                ImGui::GetWindowDrawList()->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), IM_COL32(100, 90, 90, 150), 4.0f);

                ImGui::SetCursorScreenPos(ImVec2(barPos.x + 8, barPos.y + 3));
                ImGui::Text("%s", layers2[i].name.c_str());

                float rightWidgetsWidth = 60.0f;
                ImGui::SameLine(barSize.x - rightWidgetsWidth);
                ImGui::Checkbox("##vis2", &layers2[i].visible);
                if (layers2.size() > 1) {
                    ImGui::SameLine();
                    if (ImGui::Button("X##del2")) {
                        glDeleteTextures(1, &layers2[i].textureID);
                        layers2.erase(layers2.begin() + i);
                        activeLayer2 = std::clamp(activeLayer2, 0, (int)layers2.size() - 1);
                        cacheValid = false;
                        ImGui::PopID();
                        break;
                    }
                }

                ImGui::SetCursorScreenPos(barPos);
                ImGui::InvisibleButton("layer_bar2", barSize);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    activeLayer2 = i;
                    cacheValid = false;
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
                    ImGui::SetDragDropPayload("DND_LAYER_2", &i, sizeof(int));
                    ImGui::Text("Moving layer: %s", layers2[i].name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_LAYER_2")) {
                        int srcIdx = *(const int*)payload->Data;
                        if (srcIdx != i) {
                            ImageLayer moved = layers2[srcIdx];
                            layers2.erase(layers2.begin() + srcIdx);
                            layers2.insert(layers2.begin() + i, moved);
                            activeLayer2 = i;
                            cacheValid = false;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }
        }
        ImGui::End();

        ImGui::Begin("Control Panel");
        if (ImGui::CollapsingHeader("Layer Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Image 1 Opacity", &img1Opacity, 0.0f, 1.0f);
            ImGui::SliderFloat("Image 2 Opacity", &img2Opacity, 0.0f, 1.0f);
            ImGui::Checkbox("Smooth Image 1", &smoothImage1);
            ImGui::Checkbox("Smooth Image 2", &smoothImage2);
            if (ImGui::Button("Swap Images", ImVec2(-1, 0))) {
                std::swap(layers1, layers2);
                std::swap(activeLayer1, activeLayer2);
                std::swap(img1Opacity, img2Opacity);
                std::swap(image1Only, image2Only);
                std::swap(smoothImage1, smoothImage2);
                cacheValid = false;
            }
            ImGui::Spacing();
            float availWidth = ImGui::GetContentRegionAvail().x;
            float halfWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (image1Only) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button("Image 1 Only", ImVec2(halfWidth, 0))) {
                image1Only = !image1Only;
                if (image1Only) image2Only = false;
            }
            if (image1Only) ImGui::PopStyleColor();

            ImGui::SameLine();

            if (image2Only) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button("Image 2 Only", ImVec2(halfWidth, 0))) {
                image2Only = !image2Only;
                if (image2Only) image1Only = false;
            }
            if (image2Only) ImGui::PopStyleColor();
        }

        if (ImGui::CollapsingHeader("Tile Difference Analysis", ImGuiTreeNodeFlags_DefaultOpen)) {
            tileSize = std::clamp(tileSize, minTileSize, maxTileSize);
            if (ImGui::SliderInt("Tile Size", &tileSize, minTileSize, maxTileSize)) cacheValid = false;

            const auto& img1Active = layers1[activeLayer1].data;
            int cols = (img1Active.width + tileSize - 1) / tileSize;
            int rows = (img1Active.height + tileSize - 1) / tileSize;
            int maxTiles = std::max(1, cols * rows);
            topN = std::clamp(topN, 0, maxTiles);

            ImGui::BeginDisabled(allOverlays);
            if (ImGui::SliderInt("Top N Overlays", &topN, 0, maxTiles)) cacheValid = false;
            ImGui::EndDisabled();

            if (ImGui::Checkbox("All Overlays", &allOverlays)) cacheValid = false;
            ImGui::Checkbox("Show Tile Grid", &showTileGrid);
            ImGui::SameLine();
            ImGui::ColorEdit4("##GridColor", (float*)&gridColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            const char* metrics[] = { "MSE", "MAE (L1)", "Max Difference", "SSD (Translation Search)", "SAD (Translation Search)" };
            if (ImGui::Combo("Metric", &metricType, metrics, 5)) cacheValid = false;

            bool motionVectorMode = (patternType == 4);
            if (ImGui::Checkbox("Motion Vector Mode", &motionVectorMode)) {
                patternType = motionVectorMode ? 4 : 0;
                metricType = motionVectorMode ? std::max(3, metricType) : std::min(2, metricType);
                cacheValid = false;
            }

            if (motionVectorMode) {
                const char* arrowOnly[] = { "Arrow (->)" };
                int arrowIdx = 0;
                ImGui::Combo("Pattern Gallery", &arrowIdx, arrowOnly, 1);
            } else {
                const char* standardPatterns[] = { "Cross (x)", "Plus (+)", "Circle (o)", "Diamond (<>)" };
                if (patternType > 3) patternType = 0;
                if (ImGui::Combo("Pattern Gallery", &patternType, standardPatterns, 4)) cacheValid = false;
            }

            if (ImGui::SliderFloat("Pattern Thickness", &thicknessMod, -0.8f, 0.8f)) cacheValid = false;
            if (ImGui::SliderFloat("Pattern Margin", &patternMargin, -1.0f, 1.0f)) cacheValid = false;

            const char* coloringModes[] = { "Default", "Solid Color", "Error Metric (Cool-to-Warm)" };
            ImGui::Combo("Pattern Coloring", &patternColorMode, coloringModes, 3);
            if (patternColorMode == 1) {
                ImGui::ColorEdit3("Solid Color", (float*)&solidColor, ImGuiColorEditFlags_NoInputs);
            }
        }

        if (ImGui::Button("Unload Images", ImVec2(-1, 0))) {
            TestDataGenerator::loadPresetDefault(preset1, preset2, path1, path2);
            for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
            for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
            layers1.clear(); layers2.clear();
            ImageLayer l1; l1.name = "(Synthetic Clean)"; l1.data = preset1; l1.textureID = createTextureFromData(preset1); layers1.push_back(l1);
            ImageLayer l2; l2.name = "(Synthetic Noisy)"; l2.data = preset2; l2.textureID = createTextureFromData(preset2); layers2.push_back(l2);
            activeLayer1 = 0; activeLayer2 = 0;
            updateTexture(tex1Display, layers1[0].data);
            img1Opacity = 1.0f;
            img2Opacity = 1.0f;
            image1Only = false;
            image2Only = false;
            cacheValid = false;
        }
        ImGui::End();

        auto drawTileGridOverlay = [&](ImDrawList* dList, ImVec2 imgMin, ImVec2 imgMax, int imgW, int imgH) {
            if (!showTileGrid) return;
            ImU32 gridCol = IM_COL32((int)(gridColor.x * 255), (int)(gridColor.y * 255), (int)(gridColor.z * 255), (int)(gridColor.w * 255));
            int cols = (imgW + tileSize - 1) / tileSize;
            int rows = (imgH + tileSize - 1) / tileSize;

            float scaleX = (imgMax.x - imgMin.x) / (float)imgW;
            float scaleY = (imgMax.y - imgMin.y) / (float)imgH;

            for (int c = 0; c <= cols; ++c) {
                float x = imgMin.x + (float)(c * tileSize) * scaleX;
                dList->AddLine(ImVec2(x, imgMin.y), ImVec2(x, imgMax.y), gridCol);
            }
            for (int r = 0; r <= rows; ++r) {
                float y = imgMin.y + (float)(r * tileSize) * scaleY;
                dList->AddLine(ImVec2(imgMin.x, y), ImVec2(imgMax.x, y), gridCol);
            }
        };

        if (firstFrame) ImGui::SetNextWindowFocus();
        ImGui::Begin("Pattern View");

        const auto& img1Active = layers1[activeLayer1].data;
        const auto& img2Active = layers2[activeLayer2].data;

        auto applyZoomAndAlignment = [&](int zIdx, int aIdx, ImVec2 cSize) {
            if (zIdx == 0 || zIdx == 3) zoom = 1.0f;
            else if (zIdx == 1) zoom = std::clamp(std::min(cSize.x / img1Active.width, cSize.y / img1Active.height), 0.1f, 10.0f);
            else if (zIdx == 2) zoom = 2.0f;
            else if (zIdx == 4) zoom = 0.75f;
            else if (zIdx == 5) zoom = 0.50f;
            else if (zIdx == 6) zoom = 0.25f;

            float imgW = img1Active.width * zoom;
            float imgH = img1Active.height * zoom;

            if (aIdx == 0) { panX = 0.0f; panY = 0.0f; }
            else if (aIdx == 1) { panX = cSize.x - imgW; panY = 0.0f; }
            else if (aIdx == 2) { panX = (cSize.x - imgW) * 0.5f; panY = (cSize.y - imgH) * 0.5f; }
        };

        ImVec2 rawCanvasAvail = ImGui::GetContentRegionAvail();
        rawCanvasAvail.y -= 30.0f;
        if (!initialFitDone) {
            applyZoomAndAlignment(zoomIndex, alignmentIndex, rawCanvasAvail);
            initialFitDone = true;
        }

        const char* zoomOptions[] = { "Actual Size", "Fit to Canvas", "200%", "100%", "75%", "50%", "25%" };
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("Zoom", &zoomIndex, zoomOptions, 7)) applyZoomAndAlignment(zoomIndex, alignmentIndex, rawCanvasAvail);

        ImGui::SameLine();

        const char* alignmentOptions[] = { "Upper-Left", "Upper-Right", "Center" };
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("Alignment", &alignmentIndex, alignmentOptions, 3)) applyZoomAndAlignment(zoomIndex, alignmentIndex, rawCanvasAvail);

        ImGui::Spacing();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 50) canvasSize.x = 50;
        if (canvasSize.y < 50) canvasSize.y = 50;

        ImGui::InvisibleButton("image_canvas", canvasSize);
        bool hovered = ImGui::IsItemHovered();

        if (hovered && io.KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isSelecting = true;
            selectStartPos = io.MousePos;
            selectEndPos = io.MousePos;
        }

        if (isSelecting) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                selectEndPos = io.MousePos;
            } else {
                isSelecting = false;
                float x1 = (std::min(selectStartPos.x, selectEndPos.x) - canvasPos.x - panX) / zoom;
                float y1 = (std::min(selectStartPos.y, selectEndPos.y) - canvasPos.y - panY) / zoom;
                float x2 = (std::max(selectStartPos.x, selectEndPos.x) - canvasPos.x - panX) / zoom;
                float y2 = (std::max(selectStartPos.y, selectEndPos.y) - canvasPos.y - panY) / zoom;
                float w = x2 - x1;
                float h = y2 - y1;
                if (w > 5.0f && h > 5.0f) {
                    zoom = std::clamp(std::min(canvasSize.x / w, canvasSize.y / h), 0.1f, 10.0f);
                    panX = -x1 * zoom;
                    panY = -y1 * zoom;
                }
            }
        }

        if (hovered && !io.KeyCtrl && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))) {
            panX += io.MouseDelta.x;
            panY += io.MouseDelta.y;
        }

        float scrollInput = (io.MouseWheel != 0.0f) ? io.MouseWheel : io.MouseWheelH;
        if (hovered && scrollInput != 0.0f) {
            float zoomOld = zoom;
            zoom = std::clamp(zoom * std::pow(1.05f, scrollInput * 6.0f), 0.1f, 10.0f);
            ImVec2 mPos = ImGui::GetMousePos();
            panX = mPos.x - (mPos.x - panX) * (zoom / zoomOld);
            panY = mPos.y - (mPos.y - panY) * (zoom / zoomOld);
        }

        int finalTopN = allOverlays ? std::max(1, ((img1Active.width + tileSize - 1) / tileSize) * ((img1Active.height + tileSize - 1) / tileSize)) : topN;

        bool paramsChanged = (cachedTileSize != tileSize || cachedTopN != finalTopN || cachedMetricType != metricType || cachedPatternType != patternType || cachedThicknessMod != thicknessMod || cachedPatternMargin != patternMargin);
        if (!cacheValid || paramsChanged) {
            cachedTileSize = tileSize;
            cachedTopN = finalTopN;
            cachedMetricType = metricType;
            cachedPatternType = patternType;
            cachedThicknessMod = thicknessMod;
            cachedPatternMargin = patternMargin;
            cacheValid = true;

            if (patternType == 4) {
                cachedTopTiles = computeTopTranslations(img1Active, img2Active, tileSize, finalTopN, metricType);
                float step = 2.0f * M_PI / 16.0f;
                for (int i = 0; i < 16; ++i) cachedArrowMasks[i] = generateArrowMask(tileSize, thicknessMod, patternMargin, i * step);
            } else {
                std::vector<TileInfo> standardTop = TileAnalyzer::computeTopDifferences(img1Active, img2Active, tileSize, finalTopN, metricType);
                cachedTopTiles.clear();
                for (const auto& t : standardTop) cachedTopTiles.push_back({t.x, t.y, t.score, 0, 0});

                cachedPatternMask.assign(tileSize * tileSize, 0);
                float halfThick = std::max(1.0f, (float)tileSize * 0.08f * (1.0f + thicknessMod));
                for (int ty = 0; ty < tileSize; ++ty) {
                    for (int tx = 0; tx < tileSize; ++tx) {
                        float fx = (float)tx / tileSize;
                        float fy = (float)ty / tileSize;
                        bool match = false;
                        if (patternType == 0) {
                            float inset = (patternMargin <= 0.0f) ? (0.15f - patternMargin * 0.10f) : (0.15f * (1.0f - patternMargin));
                            match = (fx >= inset && fx <= (1.0f - inset) && fy >= inset && fy <= (1.0f - inset)) &&
                                    ((std::abs((float)tx - (float)ty) <= halfThick * 1.2f) || (std::abs((float)tx + (float)ty - (float)(tileSize - 1)) <= halfThick * 1.2f));
                        } else if (patternType == 1) {
                            float plusInset = (patternMargin <= 0.0f) ? (0.20f - patternMargin * 0.05f) : (0.20f * (1.0f - patternMargin));
                            match = (std::abs((float)ty - tileSize * 0.5f) <= halfThick && fx >= plusInset && fx <= (1.0f - plusInset)) ||
                                    (std::abs((float)tx - tileSize * 0.5f) <= halfThick && fy >= plusInset && fy <= (1.0f - plusInset));
                        } else if (patternType == 2) {
                            float dx = (float)tx + 0.5f - tileSize * 0.5f;
                            float dy = (float)ty + 0.5f - tileSize * 0.5f;
                            float dist = std::sqrt(dx * dx + dy * dy);
                            float outerR = tileSize * ((patternMargin <= 0.0f) ? (0.38f + patternMargin * 0.13f) : (0.38f + patternMargin * 0.12f));
                            match = (dist >= (outerR - halfThick * 1.2f) && dist <= outerR);
                        } else if (patternType == 3) {
                            float dx = std::abs((float)tx + 0.5f - tileSize * 0.5f);
                            float dy = std::abs((float)ty + 0.5f - tileSize * 0.5f);
                            float outerR = tileSize * ((patternMargin <= 0.0f) ? (0.40f + patternMargin * 0.15f) : (0.40f + patternMargin * 0.10f));
                            match = ((dx + dy) >= (outerR - halfThick * 1.5f) && (dx + dy) <= outerR);
                        }
                        if (match) cachedPatternMask[ty * tileSize + tx] = 1;
                    }
                }
            }

            img1Render = img1Active;
            if (finalTopN > 0) {
                for (const auto& tile : cachedTopTiles) {
                    if (tile.score <= 1e-5f) continue;
                    const std::vector<uint8_t>* currentMask = &cachedPatternMask;
                    if (patternType == 4) {
                        float angle = std::atan2((float)tile.dy, (float)tile.dx);
                        int idx16 = (int)(std::round(angle / (2.0f * M_PI / 16.0f))) % 16;
                        if (idx16 < 0) idx16 += 16;
                        currentMask = &cachedArrowMasks[idx16];
                    }

                    int tileW = std::min(tileSize, img1Active.width - tile.x);
                    int tileH = std::min(tileSize, img1Active.height - tile.y);
                    for (int sy = 0; sy < tileH; ++sy) {
                        for (int sx = 0; sx < tileW; ++sx) {
                            int maskX = (int)((float)sx / tileSize * tileSize);
                            int maskY = (int)((float)sy / tileSize * tileSize);
                            maskX = std::clamp(maskX, 0, tileSize - 1);
                            maskY = std::clamp(maskY, 0, tileSize - 1);

                            if ((*currentMask)[maskY * tileSize + maskX] == 1) {
                                int ix = tile.x + sx;
                                int iy = tile.y + sy;
                                if (ix >= 0 && ix < img1Active.width && iy >= 0 && iy < img1Active.height) {
                                    img1Render.pixels[(iy * img1Active.width + ix) * 4 + 3] = 0.0f;
                                }
                            }
                        }
                    }
                }
            }
            updateTexture(tex1Display, img1Render);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        ImVec2 imageMin = ImVec2(canvasPos.x + panX, canvasPos.y + panY);
        ImVec2 imageMax = ImVec2(imageMin.x + img1Active.width * zoom, imageMin.y + img1Active.height * zoom);

        auto renderImage1 = [&](const InputData& renderImg, GLuint texDisp) {
            if (smoothImage1) {
                drawList->AddImage((ImTextureID)(intptr_t)texDisp, imageMin, imageMax, ImVec2(0,0), ImVec2(1,1), IM_COL32(255, 255, 255, img1Opacity * 255));
            } else {
                for (int y = 0; y < renderImg.height; ++y) {
                    for (int x = 0; x < renderImg.width; ++x) {
                        int idx = (y * renderImg.width + x) * 4;
                        if (renderImg.pixels[idx + 3] > 0.0f) {
                            float px = canvasPos.x + panX + (float)x * zoom;
                            float py = canvasPos.y + panY + (float)y * zoom;
                            float pSize = std::max(1.0f, zoom);
                            if (px + pSize < canvasPos.x || px > canvasPos.x + canvasSize.x || py + pSize < canvasPos.y || py > canvasPos.y + canvasSize.y) continue;
                            unsigned char r = (unsigned char)(std::clamp<float>(renderImg.pixels[idx + 0], 0.0f, 1.0f) * 255.0f);
                            unsigned char g = (unsigned char)(std::clamp<float>(renderImg.pixels[idx + 1], 0.0f, 1.0f) * 255.0f);
                            unsigned char b = (unsigned char)(std::clamp<float>(renderImg.pixels[idx + 2], 0.0f, 1.0f) * 255.0f);
                            drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), IM_COL32(r, g, b, (unsigned char)(img1Opacity * 255)));
                        }
                    }
                }
            }
        };

        if (image1Only) {
            renderImage1(img1Active, layers1[activeLayer1].textureID);
        } else if (image2Only) {
            if (smoothImage2) {
                drawList->AddImage((ImTextureID)(intptr_t)layers2[activeLayer2].textureID, imageMin, imageMax, ImVec2(0,0), ImVec2(1,1), IM_COL32(255, 255, 255, img2Opacity * 255));
            } else {
                for (int y = 0; y < img2Active.height; ++y) {
                    for (int x = 0; x < img2Active.width; ++x) {
                        float px = canvasPos.x + panX + (float)x * zoom;
                        float py = canvasPos.y + panY + (float)y * zoom;
                        float pSize = std::max(1.0f, zoom);
                        if (px + pSize < canvasPos.x || px > canvasPos.x + canvasSize.x || py + pSize < canvasPos.y || py > canvasPos.y + canvasSize.y) continue;
                        int pidx = (y * img2Active.width + x) * 4;
                        unsigned char r = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 0], 0.0f, 1.0f) * 255.0f);
                        unsigned char g = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 1], 0.0f, 1.0f) * 255.0f);
                        unsigned char b = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 2], 0.0f, 1.0f) * 255.0f);
                        drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), IM_COL32(r, g, b, (unsigned char)(img2Opacity * 255)));
                    }
                }
            }
        } else {
            renderImage1(img1Render, tex1Display);

            if (finalTopN > 0 && img2Opacity > 0.0f) {
                float minScore = 1e9f, maxScore = -1e9f;
                if (patternColorMode == 2) {
                    for (const auto& tile : cachedTopTiles) {
                        if (tile.score <= 1e-5f) continue;
                        minScore = std::min(minScore, tile.score);
                        maxScore = std::max(maxScore, tile.score);
                    }
                    if (maxScore - minScore < 1e-5f) maxScore = minScore + 1.0f;
                }

                for (const auto& tile : cachedTopTiles) {
                    if (tile.score <= 1e-5f) continue;
                    const std::vector<uint8_t>* currentMask = &cachedPatternMask;
                    if (patternType == 4) {
                        float angle = std::atan2((float)tile.dy, (float)tile.dx);
                        int idx16 = (int)(std::round(angle / (2.0f * M_PI / 16.0f))) % 16;
                        if (idx16 < 0) idx16 += 16;
                        currentMask = &cachedArrowMasks[idx16];
                    }

                    int tileW = std::min(tileSize, img1Active.width - tile.x);
                    int tileH = std::min(tileSize, img1Active.height - tile.y);
                    for (int ty = 0; ty < tileH; ++ty) {
                        for (int tx = 0; tx < tileW; ++tx) {
                            int maskX = (int)((float)tx / tileSize * tileSize);
                            int maskY = (int)((float)ty / tileSize * tileSize);
                            maskX = std::clamp(maskX, 0, tileSize - 1);
                            maskY = std::clamp(maskY, 0, tileSize - 1);

                            if ((*currentMask)[maskY * tileSize + maskX] == 1) {
                                int ix = tile.x + tx;
                                int iy = tile.y + ty;
                                if (ix >= 0 && ix < img1Active.width && iy >= 0 && iy < img1Active.height) {
                                    float px = canvasPos.x + panX + (float)ix * zoom;
                                    float py = canvasPos.y + panY + (float)iy * zoom;
                                    float pSize = std::max(1.0f, zoom);
                                    if (px + pSize < canvasPos.x || px > canvasPos.x + canvasSize.x || py + pSize < canvasPos.y || py > canvasPos.y + canvasSize.y) continue;

                                    unsigned char alphaByte = (unsigned char)(img2Opacity * 255);
                                    if (patternColorMode > 0 || !smoothImage2) {
                                        if (patternColorMode == 1) {
                                            drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), IM_COL32((int)(solidColor.x * 255), (int)(solidColor.y * 255), (int)(solidColor.z * 255), alphaByte));
                                        } else if (patternColorMode == 2) {
                                            ImVec4 cwCol = getParaViewCoolWarmColor((tile.score - minScore) / (maxScore - minScore));
                                            drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), IM_COL32((int)(cwCol.x * 255), (int)(cwCol.y * 255), (int)(cwCol.z * 255), alphaByte));
                                        } else {
                                            unsigned char r = 200, g = 80, b = 80;
                                            if (!img2Active.pixels.empty() && ix < img2Active.width && iy < img2Active.height) {
                                                int pidx = (iy * img2Active.width + ix) * 4;
                                                r = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 0], 0.0f, 1.0f) * 255.0f);
                                                g = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 1], 0.0f, 1.0f) * 255.0f);
                                                b = (unsigned char)(std::clamp<float>(img2Active.pixels[pidx + 2], 0.0f, 1.0f) * 255.0f);
                                            }
                                            drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), IM_COL32(r, g, b, alphaByte));
                                        }
                                    } else {
                                        drawList->AddImage((ImTextureID)(intptr_t)layers2[activeLayer2].textureID, ImVec2(px, py), ImVec2(px + pSize, py + pSize), ImVec2((float)ix / img2Active.width, (float)iy / img2Active.height), ImVec2((float)(ix + 1) / img2Active.width, (float)(iy + 1) / img2Active.height), IM_COL32(255, 255, 255, alphaByte));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        drawTileGridOverlay(drawList, imageMin, imageMax, img1Active.width, img1Active.height);

        if (isSelecting) {
            drawList->AddRectFilled(ImVec2(std::min(selectStartPos.x, selectEndPos.x), std::min(selectStartPos.y, selectEndPos.y)), ImVec2(std::max(selectStartPos.x, selectEndPos.x), std::max(selectStartPos.y, selectEndPos.y)), IM_COL32(100, 150, 255, 50));
            drawList->AddRect(ImVec2(std::min(selectStartPos.x, selectEndPos.x), std::min(selectStartPos.y, selectEndPos.y)), ImVec2(std::max(selectStartPos.x, selectEndPos.x), std::max(selectStartPos.y, selectEndPos.y)), IM_COL32(100, 150, 255, 200));
        }

        if (hovered) {
            ImVec2 mPos = ImGui::GetMousePos();
            float lx = (mPos.x - canvasPos.x - panX) / zoom;
            float ly = (mPos.y - canvasPos.y - panY) / zoom;
            if (lx >= 0.0f && lx < (float)img1Active.width && ly >= 0.0f && ly < (float)img1Active.height) {
                int px = (int)lx, py = (int)ly;
                ImGui::BeginTooltip();
                if (zoom >= 5.0f) {
                    int idx1 = (py * img1Active.width + px) * 4;
                    int idx2 = (std::clamp(py, 0, img2Active.height - 1) * img2Active.width + std::clamp(px, 0, img2Active.width - 1)) * 4;
                    ImGui::Text("Pixel ID: (%d, %d)", px, py);
                    ImGui::Text("IMG 1: (%.3f, %.3f, %.3f, %.3f)", img1Active.pixels[idx1], img1Active.pixels[idx1+1], img1Active.pixels[idx1+2], img1Active.pixels[idx1+3]);
                    ImGui::Text("IMG 2: (%.3f, %.3f, %.3f, %.3f)", img2Active.pixels[idx2], img2Active.pixels[idx2+1], img2Active.pixels[idx2+2], img2Active.pixels[idx2+3]);
                } else {
                    int tc = px / tileSize, tr = py / tileSize;
                    const char* mn = (metricType == 0) ? "MSE" : (metricType == 1) ? "MAE" : (metricType == 2) ? "Max Diff" : (metricType == 3) ? "SSD" : "SAD";
                    ImGui::Text("Tile ID: (%d, %d)", tc, tr);
                    if (patternType == 4) {
                        MatchResult res = computeTileTranslation(img1Active, img2Active, tc * tileSize, tr * tileSize, tileSize, metricType);
                        ImGui::Text("%s: %.2f", mn, res.score);
                        ImGui::Text("Estimated Shift: (%d, %d)", res.bestDx, res.bestDy);
                    } else {
                        float score = computeTileMetric(img1Active, img2Active, tc * tileSize, tr * tileSize, tileSize, metricType);
                        ImGui::Text("%s: %.2f", mn, score);
                    }
                }
                ImGui::EndTooltip();
            }
        }

        drawList->PopClipRect();
        ImGui::End();

        ImGui::Begin("Juxtaposed View");
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float halfW = (avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            ImGui::BeginChild("JuxtaposeImg1", ImVec2(halfW, avail.y), true);
            panel1Min = ImGui::GetWindowPos();
            panel1Max = ImVec2(panel1Min.x + ImGui::GetWindowSize().x, panel1Min.y + ImGui::GetWindowSize().y);
            ImGui::Text("Image 1 (Active Layer): %s", layers1[activeLayer1].name.c_str());
            ImGui::Separator();

            ImVec2 cs1 = ImGui::GetContentRegionAvail();
            if (cs1.x > 10 && cs1.y > 10) {
                float aspect = (float)img1Active.width / (float)img1Active.height;
                float dw = cs1.x, dh = std::max(10.0f, std::min(cs1.y - 25.0f, dw / aspect));
                dw = dh * aspect;
                ImGui::Image((ImTextureID)(intptr_t)layers1[activeLayer1].textureID, ImVec2(dw, dh));
                drawTileGridOverlay(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), img1Active.width, img1Active.height);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) openImage1(cacheValid);
                ImGui::Spacing();
                ImGui::TextDisabled("(Drop file here or double-click image)");
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("JuxtaposeImg2", ImVec2(halfW, avail.y), true);
            panel2Min = ImGui::GetWindowPos();
            panel2Max = ImVec2(panel2Min.x + ImGui::GetWindowSize().x, panel2Min.y + ImGui::GetWindowSize().y);
            ImGui::Text("Image 2 (Active Layer): %s", layers2[activeLayer2].name.c_str());
            ImGui::Separator();

            ImVec2 cs2 = ImGui::GetContentRegionAvail();
            if (cs2.x > 10 && cs2.y > 10) {
                float aspect = (float)img2Active.width / (float)img2Active.height;
                float dw = cs2.x, dh = std::max(10.0f, std::min(cs2.y - 25.0f, dw / aspect));
                dw = dh * aspect;
                ImGui::Image((ImTextureID)(intptr_t)layers2[activeLayer2].textureID, ImVec2(dw, dh));
                drawTileGridOverlay(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), img2Active.width, img2Active.height);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) openImage2(cacheValid);
                ImGui::Spacing();
                ImGui::TextDisabled("(Drop file here or double-click image)");
            }
            ImGui::EndChild();
        }
        ImGui::End();

        firstFrame = false;

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    for (auto& l : layers1) glDeleteTextures(1, &l.textureID);
    for (auto& l : layers2) glDeleteTextures(1, &l.textureID);
    glDeleteTextures(1, &tex1Display);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
