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
        m_oldSprites = m_project->GetSprites();

        if (m_oldSprites.empty() || !m_oldTexture) return;

        std::vector<Rect> rawRects;
        for (const auto& s : m_oldSprites) {
            rawRects.push_back(s->GetSourceRect());
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < rawRects.size(); ++i) {
                for (size_t j = i + 1; j < rawRects.size(); ++j) {
                    Rect& r1 = rawRects[i];
                    Rect& r2 = rawRects[j];
                    
                    float c1x = r1.x + r1.width / 2.0f;
                    float c1y = r1.y + r1.height / 2.0f;
                    float c2x = r2.x + r2.width / 2.0f;
                    float c2y = r2.y + r2.height / 2.0f;
                    
                    bool r2_in_r1 = (c2x >= r1.x && c2x <= r1.x + r1.width && c2y >= r1.y && c2y <= r1.y + r1.height);
                    bool r1_in_r2 = (c1x >= r2.x && c1x <= r2.x + r2.width && c1y >= r2.y && c1y <= r2.y + r2.height);
                    
                    if (r2_in_r1 || r1_in_r2) {
                        float minX = std::min(r1.x, r2.x);
                        float minY = std::min(r1.y, r2.y);
                        float maxX = std::max(r1.x + r1.width, r2.x + r2.width);
                        float maxY = std::max(r1.y + r1.height, r2.y + r2.height);
                        
                        r1.x = minX;
                        r1.y = minY;
                        r1.width = maxX - minX;
                        r1.height = maxY - minY;
                        
                        rawRects.erase(rawRects.begin() + j);
                        changed = true;
                        break; 
                    }
                }
                if (changed) break;
            }
        }

        std::vector<Rect> validRects;
        for (const auto& r : rawRects) {
            if (r.width > 35 && r.height > 35) {
                validRects.push_back(r);
            }
        }

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

        const auto& oldPixels = m_oldTexture->GetPixels();
        int oldW = m_oldTexture->GetWidth();

        for (size_t i = 0; i < validRects.size(); ++i) {
            const Rect& rect = validRects[i];
            
            int destX = static_cast<int>(i * maxWidth + (maxWidth - rect.width) / 2.0f);
            int destY = static_cast<int>(maxHeight - rect.height); 

            for (int y = 0; y < static_cast<int>(rect.height); ++y) {
                for (int x = 0; x < static_cast<int>(rect.width); ++x) {
                    int oldIdx = ((static_cast<int>(rect.y) + y) * oldW + (static_cast<int>(rect.x) + x)) * 4;
                    int newIdx = ((destY + y) * newWidth + (destX + x)) * 4;
                    newPixels[newIdx]   = oldPixels[oldIdx];
                    newPixels[newIdx+1] = oldPixels[oldIdx+1];
                    newPixels[newIdx+2] = oldPixels[oldIdx+2];
                    newPixels[newIdx+3] = oldPixels[oldIdx+3];
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