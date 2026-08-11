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

        std::vector<std::shared_ptr<SpriteDefinition>> sortedSprites = m_oldSprites;
        std::sort(sortedSprites.begin(), sortedSprites.end(), [](const auto& a, const auto& b) {
            auto rA = a->GetSourceRect();
            auto rB = b->GetSourceRect();
            float threshold = std::min(rA.height, rB.height) / 2.0f;
            if (std::abs(rA.y - rB.y) > threshold) {
                return rA.y < rB.y; 
            }
            return rA.x < rB.x; 
        });

        float maxWidth = 0.0f;
        float maxHeight = 0.0f;
        for (const auto& s : sortedSprites) {
            auto r = s->GetSourceRect();
            if (r.width > maxWidth) maxWidth = r.width;
            if (r.height > maxHeight) maxHeight = r.height;
        }

        int newWidth = static_cast<int>(maxWidth * sortedSprites.size());
        int newHeight = static_cast<int>(maxHeight);
        std::vector<uint8_t> newPixels(newWidth * newHeight * 4, 0); 

        const auto& oldPixels = m_oldTexture->GetPixels();
        int oldW = m_oldTexture->GetWidth();

        for (size_t i = 0; i < sortedSprites.size(); ++i) {
            auto rect = sortedSprites[i]->GetSourceRect();
            
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

    // Cast away the constness to satisfy the Project's Setter
    m_project->SetTexture(std::const_pointer_cast<SourceTexture>(m_newTexture));
    m_project->SetSprites(m_newSprites); 
}

void RepackFramesCommand::Undo() {
    if (m_project && m_oldTexture) {
        // Cast away the constness to satisfy the Project's Setter
        m_project->SetTexture(std::const_pointer_cast<SourceTexture>(m_oldTexture));
        m_project->SetSprites(m_oldSprites);
    }
}

}