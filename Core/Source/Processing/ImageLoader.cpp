#include "Processing/ImageLoader.h"
#include "DataModels/SourceTexture.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb_image.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace StudioCore {

std::shared_ptr<SourceTexture> ImageLoader::LoadFromFile(const std::string& filePath, std::string& outErrorMessage) {
    int width = 0;
    int height = 0;
    int channels = 0;

    // Force 4 channels (RGBA)
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
            newPixels[idx + 3] = 0; // Make transparent
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

    // Helper lambda to add unique colors to our background palette
    auto addBgColor = [&](uint8_t r, uint8_t g, uint8_t b) {
        for (const auto& c : bgPalette) {
            float dist = std::sqrt(std::pow(c.r - r, 2) + std::pow(c.g - g, 2) + std::pow(c.b - b, 2));
            if (dist < 15.0f) return; // Color is already represented in the palette
        }
        bgPalette.push_back({r, g, b});
    };

    // 1. Scan the perimeter of the image to collect all background colors (including watermarks)
    for (int x = 0; x < width; x += 4) { // Step by 4 to optimize
        addBgColor(origPixels[(0 * width + x) * 4], origPixels[(0 * width + x) * 4 + 1], origPixels[(0 * width + x) * 4 + 2]);
        addBgColor(origPixels[((height - 1) * width + x) * 4], origPixels[((height - 1) * width + x) * 4 + 1], origPixels[((height - 1) * width + x) * 4 + 2]);
    }
    for (int y = 0; y < height; y += 4) {
        addBgColor(origPixels[(y * width + 0) * 4], origPixels[(y * width + 0) * 4 + 1], origPixels[(y * width + 0) * 4 + 2]);
        addBgColor(origPixels[(y * width + width - 1) * 4], origPixels[(y * width + width - 1) * 4 + 1], origPixels[(y * width + width - 1) * 4 + 2]);
    }

    // Explicitly add pure white just in case the watermark didn't touch the very edge
    addBgColor(255, 255, 255);

    // Increase tolerance slightly to catch heavy JPEG artifacts 
    float aggressiveTolerance = 55.0f;

    // 2. Wipe out anything that matches the background palette
    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t pR = newPixels[idx];
        uint8_t pG = newPixels[idx+1];
        uint8_t pB = newPixels[idx+2];

        bool isBackground = false;
        for (const auto& bgC : bgPalette) {
            float dist = std::sqrt(std::pow(pR - bgC.r, 2) + std::pow(pG - bgC.g, 2) + std::pow(pB - bgC.b, 2));
            if (dist <= aggressiveTolerance) {
                isBackground = true;
                break;
            }
        }

        if (isBackground) {
            newPixels[idx] = 0;
            newPixels[idx+1] = 0;
            newPixels[idx+2] = 0;
            newPixels[idx+3] = 0; // Pure transparency
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

}