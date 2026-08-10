#include "Processing/ImageLoader.h"
#include "DataModels/SourceTexture.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb_image.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>

namespace StudioCore {

std::shared_ptr<SourceTexture> ImageLoader::LoadFromFile(const std::string& filePath, std::string& outErrorMessage) {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* rawPixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!rawPixels) {
        outErrorMessage = std::string("Failed to load image from path '") + filePath + "': " + stbi_failure_reason();
        return nullptr;
    }

    if (width <= 0 || height <= 0) {
        outErrorMessage = "Invalid image dimensions in path: " + filePath;
        stbi_image_free(rawPixels);
        return nullptr;
    }

    size_t pixelBufferSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    std::vector<uint8_t> pixelData(rawPixels, rawPixels + pixelBufferSize);

    stbi_image_free(rawPixels);
    outErrorMessage.clear();
    return std::make_shared<SourceTexture>(width, height, std::move(pixelData));
}

std::shared_ptr<SourceTexture> ImageLoader::ChromaKey(const SourceTexture& source, uint8_t r, uint8_t g, uint8_t b, float tolerance) {
    int width = source.GetWidth();
    int height = source.GetHeight();
    const auto& origPixels = source.GetPixels();
    std::vector<uint8_t> newPixels = origPixels;

    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        float dist = std::sqrt(std::pow(newPixels[idx] - r, 2) + 
                               std::pow(newPixels[idx+1] - g, 2) + 
                               std::pow(newPixels[idx+2] - b, 2));

        if (dist <= tolerance) {
            newPixels[idx] = 0;
            newPixels[idx+1] = 0;
            newPixels[idx+2] = 0;
            newPixels[idx + 3] = 0; 
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

struct ColorRGB { uint8_t r, g, b; };

std::shared_ptr<SourceTexture> ImageLoader::RemoveFakeCheckerboard(const SourceTexture& source, float tolerance) {
    int width = source.GetWidth();
    int height = source.GetHeight();
    const auto& origPixels = source.GetPixels();
    std::vector<uint8_t> newPixels = origPixels;

    if (width < 8 || height < 8) return std::make_shared<SourceTexture>(width, height, std::move(newPixels));

    std::vector<ColorRGB> bgPalette;

    auto addBgColor = [&](uint8_t r, uint8_t g, uint8_t b) {
        for (const auto& c : bgPalette) {
            float dist = std::sqrt(std::pow(c.r - r, 2) + std::pow(c.g - g, 2) + std::pow(c.b - b, 2));
            if (dist < 15.0f) return; 
        }
        bgPalette.push_back({r, g, b});
    };

    // 1. Build background palette from perimeter
    for (int x = 0; x < width; x += 4) { 
        addBgColor(origPixels[(0 * width + x) * 4], origPixels[(0 * width + x) * 4 + 1], origPixels[(0 * width + x) * 4 + 2]);
        addBgColor(origPixels[((height - 1) * width + x) * 4], origPixels[((height - 1) * width + x) * 4 + 1], origPixels[((height - 1) * width + x) * 4 + 2]);
    }
    for (int y = 0; y < height; y += 4) {
        addBgColor(origPixels[(y * width + 0) * 4], origPixels[(y * width + 0) * 4 + 1], origPixels[(y * width + 0) * 4 + 2]);
        addBgColor(origPixels[(y * width + width - 1) * 4], origPixels[(y * width + width - 1) * 4 + 1], origPixels[(y * width + width - 1) * 4 + 2]);
    }
    addBgColor(255, 255, 255); // Explicitly add white for watermarks

    // 2. Identify all candidate background pixels globally
    float aggressiveTolerance = 50.0f;
    std::vector<bool> isBgCandidate(width * height, false);

    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t pR = origPixels[idx];
        uint8_t pG = origPixels[idx+1];
        uint8_t pB = origPixels[idx+2];

        for (const auto& bgC : bgPalette) {
            float dist = std::sqrt(std::pow(pR - bgC.r, 2) + std::pow(pG - bgC.g, 2) + std::pow(pB - bgC.b, 2));
            if (dist <= aggressiveTolerance) {
                isBgCandidate[i] = true;
                break;
            }
        }
    }

    // 3. Group candidates into "Islands" using 8-way connectivity (so diagonal checkers link up)
    std::vector<bool> visited(width * height, false);
    
    // 8-way directions
    const int dx[] = {1, 1, 1, 0, -1, -1, -1, 0};
    const int dy[] = {-1, 0, 1, 1, 1, 0, -1, -1};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (isBgCandidate[y * width + x] && !visited[y * width + x]) {
                
                std::vector<std::pair<int, int>> islandPixels;
                std::queue<std::pair<int, int>> q;
                
                q.push({x, y});
                visited[y * width + x] = true;
                islandPixels.push_back({x, y});
                
                bool touchesEdge = false;

                // Flood fill the current island
                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();

                    if (cx == 0 || cx == width - 1 || cy == 0 || cy == height - 1) {
                        touchesEdge = true;
                    }

                    for (int i = 0; i < 8; ++i) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];

                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int nIdx = ny * width + nx;
                            if (isBgCandidate[nIdx] && !visited[nIdx]) {
                                visited[nIdx] = true;
                                q.push({nx, ny});
                                islandPixels.push_back({nx, ny});
                            }
                        }
                    }
                }

                // 4. Protection Logic:
                // An ape's eye is very small (usually < 20 pixels). 
                // The gap between legs is large (well over 50 pixels).
                // If it touches the edge OR is larger than 40 pixels, nuke it.
                if (touchesEdge || islandPixels.size() > 40) {
                    for (const auto& p : islandPixels) {
                        int pIdx = (p.second * width + p.first) * 4;
                        newPixels[pIdx] = 0;
                        newPixels[pIdx+1] = 0;
                        newPixels[pIdx+2] = 0;
                        newPixels[pIdx+3] = 0; // Pure transparency
                    }
                }
            }
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

}