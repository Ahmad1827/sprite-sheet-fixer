#pragma once
#include "Commands/ICommand.h"
#include "DataModels/SourceTexture.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

namespace StudioCore {

// Stores only the bounding box of the pixels that differ between oldPixels and
// newPixels, so a small edit on a large sheet does not keep two full copies.
class PixelRegionCommand : public ICommand {
public:
    PixelRegionCommand(std::shared_ptr<SourceTexture> texture,
                       const std::vector<uint8_t>& oldPixels,
                       const std::vector<uint8_t>& newPixels)
        : m_texture(texture) {
        if (!m_texture) return;

        const int w = m_texture->GetWidth();
        const int h = m_texture->GetHeight();
        const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;

        if (w <= 0 || h <= 0 || oldPixels.size() != expected || newPixels.size() != expected) {
            m_isFullImage = true;
            m_oldPixels = oldPixels;
            m_newPixels = newPixels;
            return;
        }

        const size_t rowBytes = static_cast<size_t>(w) * 4;
        int minX = w, minY = h, maxX = -1, maxY = -1;
        for (int y = 0; y < h; ++y) {
            const uint8_t* a = oldPixels.data() + y * rowBytes;
            const uint8_t* b = newPixels.data() + y * rowBytes;
            if (std::memcmp(a, b, rowBytes) == 0) continue;

            int first = 0;
            while (std::memcmp(a + first * 4, b + first * 4, 4) == 0) ++first;
            int last = w - 1;
            while (std::memcmp(a + last * 4, b + last * 4, 4) == 0) --last;

            if (first < minX) minX = first;
            if (last > maxX) maxX = last;
            if (minY == h) minY = y;
            maxY = y;
        }

        if (maxY < 0) return;

        m_x = minX;
        m_y = minY;
        m_width = maxX - minX + 1;
        m_height = maxY - minY + 1;

        const size_t regionRowBytes = static_cast<size_t>(m_width) * 4;
        m_oldPixels.resize(regionRowBytes * m_height);
        m_newPixels.resize(regionRowBytes * m_height);
        for (int row = 0; row < m_height; ++row) {
            const size_t src = (static_cast<size_t>(m_y + row) * w + m_x) * 4;
            std::memcpy(m_oldPixels.data() + row * regionRowBytes, oldPixels.data() + src, regionRowBytes);
            std::memcpy(m_newPixels.data() + row * regionRowBytes, newPixels.data() + src, regionRowBytes);
        }
    }

    void Execute() override {
        Apply(m_newPixels);
    }

    void Undo() override {
        Apply(m_oldPixels);
    }

private:
    void Apply(const std::vector<uint8_t>& region) {
        if (!m_texture) return;
        if (m_isFullImage) {
            m_texture->SetPixels(region);
            return;
        }
        if (m_width <= 0 || m_height <= 0) return;

        const int w = m_texture->GetWidth();
        auto& pixels = m_texture->GetPixelsMutable();
        const size_t regionRowBytes = static_cast<size_t>(m_width) * 4;
        for (int row = 0; row < m_height; ++row) {
            const size_t dst = (static_cast<size_t>(m_y + row) * w + m_x) * 4;
            if (dst + regionRowBytes > pixels.size()) return;
            std::memcpy(pixels.data() + dst, region.data() + row * regionRowBytes, regionRowBytes);
        }
    }

    std::shared_ptr<SourceTexture> m_texture;
    std::vector<uint8_t> m_oldPixels;
    std::vector<uint8_t> m_newPixels;
    int m_x{0};
    int m_y{0};
    int m_width{0};
    int m_height{0};
    bool m_isFullImage{false};
};

}
