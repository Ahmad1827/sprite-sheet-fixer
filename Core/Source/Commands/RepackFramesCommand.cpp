#include "Commands/RepackFramesCommand.h"
#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/SpriteDefinition.h"
#include <algorithm>
#include <cmath>
#include <queue>

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

        std::vector<bool> visited(w * h, false);
        std::vector<Rect> rawRects;

        const int dx[] = {1, 1, 1, 0, -1, -1, -1, 0};
        const int dy[] = {-1, 0, 1, 1, 1, 0, -1, -1};

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (pixels[(y * w + x) * 4 + 3] > 0 && !visited[y * w + x]) {
                    int minX = x, maxX = x, minY = y, maxY = y;
                    std::queue<std::pair<int, int>> q;
                    
                    q.push({x, y});
                    visited[y * w + x] = true;
                    
                    while (!q.empty()) {
                        auto [cx, cy] = q.front();
                        q.pop();
                        
                        if (cx < minX) minX = cx;
                        if (cx > maxX) maxX = cx;
                        if (cy < minY) minY = cy;
                        if (cy > maxY) maxY = cy;
                        
                        for (int i = 0; i < 8; ++i) {
                            int nx = cx + dx[i];
                            int ny = cy + dy[i];
                            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                if (pixels[(ny * w + nx) * 4 + 3] > 0 && !visited[ny * w + nx]) {
                                    visited[ny * w + nx] = true;
                                    q.push({nx, ny});
                                }
                            }
                        }
                    }
                    rawRects.push_back({static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(maxX - minX + 1), static_cast<float>(maxY - minY + 1)});
                }
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < rawRects.size(); ++i) {
                for (size_t j = i + 1; j < rawRects.size(); ++j) {
                    Rect& r1 = rawRects[i];
                    Rect& r2 = rawRects[j];
                    
                    float m = 10.0f;
                    bool nearIntersect = (r1.x - m <= r2.x + r2.width && 
                                          r1.x + r1.width + m >= r2.x &&
                                          r1.y - m <= r2.y + r2.height && 
                                          r1.y + r1.height + m >= r2.y);
                    
                    if (nearIntersect) {
                        float area1 = r1.width * r1.height;
                        float area2 = r2.width * r2.height;
                        
                        if (std::min(area1, area2) < std::max(area1, area2) * 0.4f) {
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
                }
                if (changed) break;
            }
        }

        std::vector<Rect> validRects;
        for (const auto& r : rawRects) {
            if (r.width > 20 && r.height > 20) {
                validRects.push_back(r);
            }
        }

        m_oldSprites.clear();
        for (size_t i = 0; i < validRects.size(); ++i) {
             m_oldSprites.push_back(std::make_shared<SpriteDefinition>("old_" + std::to_string(i), validRects[i]));
        }

        if (validRects.empty()) return;

        std::sort(validRects.begin(), validRects.end(), [](const Rect& a, const Rect& b) {
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