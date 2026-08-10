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

    // Copy raw C-buffer to std::vector for safe RAII memory ownership
    size_t pixelBufferSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    std::vector<uint8_t> pixelData(rawPixels, rawPixels + pixelBufferSize);

    // Free the STB allocated raw memory immediately
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

std::shared_ptr<SourceTexture> ImageLoader::RemoveFakeCheckerboard(const SourceTexture& source, float tolerance) {
    int width = source.GetWidth();
    int height = source.GetHeight();
    const auto& origPixels = source.GetPixels();
    std::vector<uint8_t> newPixels = origPixels;

    if (width < 8 || height < 8) return std::make_shared<SourceTexture>(width, height, std::move(newPixels));

    // Sample Color A from Top-Left (0,0)
    uint8_t aR = origPixels[0];
    uint8_t aG = origPixels[1];
    uint8_t aB = origPixels[2];

    // Search a small 8x8 block for the alternating checkerboard color (Color B)
    uint8_t bR = aR, bG = aG, bB = aB;
    bool foundB = false;

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int idx = (y * width + x) * 4;
            float dist = std::sqrt(std::pow(origPixels[idx] - aR, 2) + 
                                   std::pow(origPixels[idx+1] - aG, 2) + 
                                   std::pow(origPixels[idx+2] - aB, 2));
            
            // If it's a noticeably different color but not wildly different (likely the other gray square)
            if (dist > 10.0f && dist < 100.0f) { 
                bR = origPixels[idx];
                bG = origPixels[idx+1];
                bB = origPixels[idx+2];
                foundB = true;
                break;
            }
        }
        if (foundB) break;
    }

    // Apply tolerance-based removal for both background colors
    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t pR = newPixels[idx];
        uint8_t pG = newPixels[idx+1];
        uint8_t pB = newPixels[idx+2];

        float distA = std::sqrt(std::pow(pR - aR, 2) + std::pow(pG - aG, 2) + std::pow(pB - aB, 2));
        float distB = foundB ? std::sqrt(std::pow(pR - bR, 2) + std::pow(pG - bG, 2) + std::pow(pB - bB, 2)) : 9999.0f;

        // Tolerance of ~35 usually catches the watermark lines and JPEG artifacts too
        if (distA <= tolerance || distB <= tolerance) {
            newPixels[idx] = 0;
            newPixels[idx+1] = 0;
            newPixels[idx+2] = 0;
            newPixels[idx+3] = 0; // Pure transparency
        }
    }

    return std::make_shared<SourceTexture>(width, height, std::move(newPixels));
}

}