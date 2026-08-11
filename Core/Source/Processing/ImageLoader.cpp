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

std::shared_ptr<SourceTexture> ImageLoader::RemoveFakeCheckerboard(const SourceTexture& source, float tolerance) {
    int width = source.GetWidth();
    int height = source.GetHeight();
    const auto& origPixels = source.GetPixels();
    std::vector<uint8_t> newPixels = origPixels;

    if (width < 8 || height < 8) return std::make_shared<SourceTexture>(width, height, std::move(newPixels));

    // 1. DETERMINISTIC NEUTRAL-COLOR MATH
    // Backgrounds (Grays, Blacks, Whites) have very low variance between their R, G, B channels.
    // Monkeys (Browns, Oranges) have high variance.
    auto isBgColor = [](uint8_t r, uint8_t g, uint8_t b) {
        int mx = std::max({r, g, b});
        int mn = std::min({r, g, b});
        int diff = mx - mn;

        // Rule A: Checkerboard & Watermarks (Mid-to-Bright Neutral Grays/Whites)
        if (mx > 40 && diff <= 20) return true;

        // Rule B: AI Black Artifacts (Very dark, strict neutral)
        // A dark brown monkey shadow will have diff > 8, so it is safely ignored!
        if (mx <= 40 && diff <= 8) return true;

        return false;
    };

    // Flag all candidates
    std::vector<bool> isBgCandidate(width * height, false);
    for (int i = 0; i < width * height; ++i) {
        isBgCandidate[i] = isBgColor(origPixels[i * 4], origPixels[i * 4 + 1], origPixels[i * 4 + 2]);
    }

    // 2. ISLAND GROUPING (8-Way Connectivity)
    std::vector<bool> visited(width * height, false);
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

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();

                    if (cx == 0 || cx == width - 1 || cy == 0 || cy == height - 1) touchesEdge = true;

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

                // 3. PROTECTION LOGIC
                // Nuke it if it touches the edge OR if it's a large chunk of trapped checkerboard
                if (touchesEdge || islandPixels.size() > 25) {
                    for (const auto& p : islandPixels) {
                        int pIdx = (p.second * width + p.first) * 4;
                        newPixels[pIdx] = 0;
                        newPixels[pIdx+1] = 0;
                        newPixels[pIdx+2] = 0;
                        newPixels[pIdx+3] = 0; // Transparent
                    }
                }
            }
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

}