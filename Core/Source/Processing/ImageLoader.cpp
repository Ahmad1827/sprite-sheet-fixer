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

    if (width < 2 || height < 2) return std::make_shared<SourceTexture>(width, height, std::move(newPixels));

    struct ColorRGB {
        float r, g, b;
    };

    std::vector<ColorRGB> bgPalette;

    int sampleRows = std::max(2, std::min(height / 10, 16));
    for (int y = 0; y < sampleRows; ++y) {
        for (int x = 0; x < width; x += 4) {
            int idx = (y * width + x) * 4;
            if (origPixels[idx + 3] == 0) continue;

            float pr = origPixels[idx];
            float pg = origPixels[idx + 1];
            float pb = origPixels[idx + 2];

            bool matched = false;
            for (auto& bg : bgPalette) {
                float dr = pr - bg.r;
                float dg = pg - bg.g;
                float db = pb - bg.b;
                if (std::sqrt(dr * dr + dg * dg + db * db) < 18.0f) {
                    bg.r = (bg.r + pr) * 0.5f;
                    bg.g = (bg.g + pg) * 0.5f;
                    bg.b = (bg.b + pb) * 0.5f;
                    matched = true;
                    break;
                }
            }

            if (!matched && bgPalette.size() < 16) {
                bgPalette.push_back({pr, pg, pb});
            }
        }
    }

    if (bgPalette.empty()) {
        return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
    }

    auto isMatchingBg = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> bool {
        if (a == 0) return true;
        for (const auto& bg : bgPalette) {
            float dr = r - bg.r;
            float dg = g - bg.g;
            float db = b - bg.b;
            if (std::sqrt(dr * dr + dg * dg + db * db) <= 22.0f) {
                return true;
            }
        }
        return false;
    };

    std::vector<bool> visited(width * height, false);
    std::queue<std::pair<int, int>> q;

    auto tryPush = [&](int x, int y) {
        int idx = y * width + x;
        if (!visited[idx]) {
            int pIdx = idx * 4;
            if (isMatchingBg(origPixels[pIdx], origPixels[pIdx + 1], origPixels[pIdx + 2], origPixels[pIdx + 3])) {
                visited[idx] = true;
                q.push({x, y});
            }
        }
    };

    for (int x = 0; x < width; ++x) {
        tryPush(x, 0);
    }
    for (int y = 0; y < height; ++y) {
        tryPush(0, y);
        tryPush(width - 1, y);
    }

    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        int pIdx = (cy * width + cx) * 4;
        newPixels[pIdx] = 0;
        newPixels[pIdx + 1] = 0;
        newPixels[pIdx + 2] = 0;
        newPixels[pIdx + 3] = 0;

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nIdx = ny * width + nx;
                if (!visited[nIdx]) {
                    int npIdx = nIdx * 4;
                    if (isMatchingBg(origPixels[npIdx], origPixels[npIdx + 1], origPixels[npIdx + 2], origPixels[npIdx + 3])) {
                        visited[nIdx] = true;
                        q.push({nx, ny});
                    }
                }
            }
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

}