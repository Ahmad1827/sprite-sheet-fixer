#include "Commands/RepackFramesCommand.h"
#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/SpriteDefinition.h"
#include <algorithm>
#include <cmath>

namespace StudioCore {

RepackFramesCommand::RepackFramesCommand(std::shared_ptr<Project> project)
    : m_project(project) {}

void RepackFramesCommand::Execute() {
    if (!m_newTexture) {
        m_oldTexture = m_project->GetTexture();
        if (!m_oldTexture) return;
        
        int w = m_oldTexture->GetWidth();
        int h = m_oldTexture->GetHeight();
        const auto& pixels = m_oldTexture->GetPixels();

        std::vector<Rect> detectedRects;

        std::vector<bool> rowHasAlpha(h, false);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (pixels[(y * w + x) * 4 + 3] > 0) {
                    rowHasAlpha[y] = true;
                    break;
                }
            }
        }

        std::vector<std::pair<int, int>> rowBands;
        int startY = -1;
        for (int y = 0; y < h; ++y) {
            if (rowHasAlpha[y] && startY == -1) startY = y;
            else if (!rowHasAlpha[y] && startY != -1) {
                rowBands.push_back({startY, y - 1});
                startY = -1;
            }
        }
        if (startY != -1) rowBands.push_back({startY, h - 1});

        for (const auto& band : rowBands) {
            int rStartY = band.first;
            int rEndY = band.second;

            std::vector<bool> colHasAlpha(w, false);
            for (int x = 0; x < w; ++x) {
                for (int y = rStartY; y <= rEndY; ++y) {
                    if (pixels[(y * w + x) * 4 + 3] > 0) {
                        colHasAlpha[x] = true;
                        break;
                    }
                }
            }

            int startX = -1;
            for (int x = 0; x < w; ++x) {
                if (colHasAlpha[x] && startX == -1) startX = x;
                else if (!colHasAlpha[x] && startX != -1) {
                    detectedRects.push_back({static_cast<float>(startX), static_cast<float>(rStartY), static_cast<float>(x - startX), static_cast<float>(rEndY - rStartY + 1)});
                    startX = -1;
                }
            }
            if (startX != -1) {
                detectedRects.push_back({static_cast<float>(startX), static_cast<float>(rStartY), static_cast<float>(w - startX), static_cast<float>(rEndY - rStartY + 1)});
            }
        }

        std::vector<Rect> validRects;
        for (auto& rect : detectedRects) {
            int minX = rect.x + rect.width, maxX = rect.x;
            int minY = rect.y + rect.height, maxY = rect.y;
            bool found = false;

            for (int y = rect.y; y < rect.y + rect.height; ++y) {
                for (int x = rect.x; x < rect.x + rect.width; ++x) {
                    if (pixels[(y * w + x) * 4 + 3] > 0) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                        found = true;
                    }
                }
            }
            if (found && (maxX - minX > 25) && (maxY - minY > 25)) {
                validRects.push_back({static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(maxX - minX + 1), static_cast<float>(maxY - minY + 1)});
            }
        }
        
        m_oldSprites.clear();
        for (size_t i = 0; i < validRects.size(); ++i) {
             m_oldSprites.push_back(std::make_shared<SpriteDefinition>("old_" + std::to_string(i), validRects[i]));
        }

        if (validRects.empty()) return;

        std::sort(validRects.begin(), validRects.end(), [](const Rect& a, const Rect& b) {
            float threshold = std::min(a.height, b.height) / 2.0f;
            if (std::abs(a.y - b.y) > threshold) {
                return a.y < b.y; 
            }
            return a.x < b.x; 
        });

        float maxWidth = 0.0f;
        float maxHeight = 0.0f;
        for (const auto& r : validRects) {
            if (r.width > maxWidth) maxWidth = r.width;
            if (r.height > maxHeight) maxHeight = r.height;
        }

        int newWidth = static_cast<int>(maxWidth * validRects.size());
        int newHeight = static_cast<int>(maxHeight);
        std::vector<uint8_t> newPixels(newWidth * newHeight * 4, 0); 

        for (size_t i = 0; i < validRects.size(); ++i) {
            const Rect& rect = validRects[i];
            
            int destX = static_cast<int>(i * maxWidth + (maxWidth - rect.width) / 2.0f);
            int destY = static_cast<int>(maxHeight - rect.height); 

            for (int y = 0; y < static_cast<int>(rect.height); ++y) {
                for (int x = 0; x < static_cast<int>(rect.width); ++x) {
                    int oldIdx = ((static_cast<int>(rect.y) + y) * w + (static_cast<int>(rect.x) + x)) * 4;
                    int newIdx = ((destY + y) * newWidth + (destX + x)) * 4;
                    newPixels[newIdx]   = pixels[oldIdx];
                    newPixels[newIdx+1] = pixels[oldIdx+1];
                    newPixels[newIdx+2] = pixels[oldIdx+2];
                    newPixels[newIdx+3] = pixels[oldIdx+3];
                }
            }
            
            Rect newRect{ static_cast<float>(i * maxWidth), 0.0f, maxWidth, maxHeight };
            auto def = std::make_shared<SpriteDefinition>("frame_" + std::to_string(i), newRect);
            m_newSprites.push_back(def);
        }

        m_newTexture = std::make_shared<SourceTexture>(newWidth, newHeight, std::move(newPixels));
    }

    m_project->SetTexture(std::const_pointer_cast<SourceTexture>(m_newTexture));
    m_project->SetSprites(m_newSprites); 
}

void RepackFramesCommand::Undo() {
    if (m_project && m_oldTexture) {
        m_project->SetTexture(std::const_pointer_cast<SourceTexture>(m_oldTexture));
        m_project->SetSprites(m_oldSprites);
    }
}

}