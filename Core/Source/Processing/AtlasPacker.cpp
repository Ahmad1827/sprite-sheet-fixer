#include "Processing/AtlasPacker.h"
#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/SpriteDefinition.h"
#include "Processing/SpriteAligner.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace StudioCore {

sf::Image AtlasPacker::PackUniformGrid(const Project& project, int padding) {
    auto texture = project.GetTexture();
    auto originalSprites = project.GetSprites();

    if (!texture || originalSprites.empty()) {
        return sf::Image();
    }

    std::vector<std::shared_ptr<SpriteDefinition>> sprites = originalSprites;
    std::sort(sprites.begin(), sprites.end(), [](const auto& a, const auto& b) {
        const auto& rA = a->GetSourceRect();
        const auto& rB = b->GetSourceRect();
        
        int centerYA = rA.y + rA.height / 2;
        int centerYB = rB.y + rB.height / 2;
        
        int threshold = std::min(rA.height, rB.height) / 2;
        
        if (std::abs(centerYA - centerYB) > threshold) {
            return centerYA < centerYB; 
        }
        return rA.x < rB.x; 
    });

    std::vector<std::vector<std::shared_ptr<SpriteDefinition>>> grid;
    grid.push_back({sprites[0]});
    
    for (size_t i = 1; i < sprites.size(); ++i) {
        const auto& currentSprite = sprites[i];
        const auto& lastSprite = grid.back().back();
        
        const auto& rCurr = currentSprite->GetSourceRect();
        const auto& rLast = lastSprite->GetSourceRect();
        
        int currCenterY = rCurr.y + rCurr.height / 2;
        int lastCenterY = rLast.y + rLast.height / 2;
        int threshold = std::min(rCurr.height, rLast.height) / 2;
        
        if (std::abs(currCenterY - lastCenterY) > threshold) {
            grid.push_back({currentSprite}); 
        } else {
            grid.back().push_back(currentSprite); 
        }
    }

    float maxLeft = 0.0f;
    float maxRight = 0.0f;
    float maxUp = 0.0f;
    float maxDown = 0.0f;
    size_t maxCols = 0;

    for (const auto& row : grid) {
        maxCols = std::max(maxCols, row.size());
        for (const auto& sprite : row) {
            AlignmentResult align = SpriteAligner::ComputeAlignment(*sprite);
            const auto& rect = sprite->GetSourceRect();

            float left = rect.width * align.alignedPivot.x;
            float right = rect.width - left;
            float up = rect.height - align.alignedBaseline;
            float down = align.alignedBaseline;

            maxLeft = std::max(maxLeft, left);
            maxRight = std::max(maxRight, right);
            maxUp = std::max(maxUp, up);
            maxDown = std::max(maxDown, down);
        }
    }

    int cellWidth = static_cast<int>(std::ceil(maxLeft + maxRight)) + padding * 2;
    int cellHeight = static_cast<int>(std::ceil(maxUp + maxDown)) + padding * 2;

    int cellPivotX = padding + static_cast<int>(std::ceil(maxLeft));
    int cellPivotY = padding + static_cast<int>(std::ceil(maxUp));

    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(maxCols);

    sf::Image exportImg;
    exportImg.create(cols * cellWidth, rows * cellHeight, sf::Color::Transparent);

    sf::Image sourceImg;
    sourceImg.create(texture->GetWidth(), texture->GetHeight(), texture->GetPixels().data());

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < static_cast<int>(grid[r].size()); ++c) {
            const auto& sprite = grid[r][c];
            AlignmentResult align = SpriteAligner::ComputeAlignment(*sprite);
            const auto& rect = sprite->GetSourceRect();

            float left = rect.width * align.alignedPivot.x;
            float up = rect.height - align.alignedBaseline;

            int destX = c * cellWidth + cellPivotX - static_cast<int>(std::round(left));
            int destY = r * cellHeight + cellPivotY - static_cast<int>(std::round(up));

            exportImg.copy(sourceImg, destX, destY, sf::IntRect(rect.x, rect.y, rect.width, rect.height), true);
        }
    }

    return exportImg;
}

}