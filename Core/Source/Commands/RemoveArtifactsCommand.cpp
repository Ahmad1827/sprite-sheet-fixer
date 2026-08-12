#include "Commands/RemoveArtifactsCommand.h"
#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include <queue>
#include <map>
#include <tuple>

namespace StudioCore {

RemoveArtifactsCommand::RemoveArtifactsCommand(std::shared_ptr<Project> project, bool autoDetect)
    : m_project(project), m_autoDetect(autoDetect) {}

void RemoveArtifactsCommand::Execute() {
    if (!m_executed) {
        m_oldTexture = m_project->GetTexture();
        if (!m_oldTexture) return;

        int w = m_oldTexture->GetWidth();
        int h = m_oldTexture->GetHeight();
        std::vector<uint8_t> pixels = m_oldTexture->GetPixels();
        std::vector<bool> mask(w * h, false);

        if (m_autoDetect) {
            std::vector<bool> visited(w * h, false);
            const int dx[] = {1, 1, 1, 0, -1, -1, -1, 0};
            const int dy[] = {-1, 0, 1, 1, 1, 0, -1, -1};

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int idx = (y * w + x) * 4;
                    if (!visited[y * w + x] && pixels[idx + 3] > 0 &&
                        pixels[idx] > 240 && pixels[idx + 1] > 240 && pixels[idx + 2] > 240) {
                        
                        std::vector<std::pair<int, int>> component;
                        std::queue<std::pair<int, int>> q;
                        q.push({x, y});
                        visited[y * w + x] = true;
                        
                        while (!q.empty()) {
                            auto [cx, cy] = q.front();
                            q.pop();
                            component.push_back({cx, cy});
                            
                            for (int i = 0; i < 8; ++i) {
                                int nx = cx + dx[i];
                                int ny = cy + dy[i];
                                if (nx >= 0 && nx < w && ny >= 0 && ny < h && !visited[ny * w + nx]) {
                                    int nIdx = (ny * w + nx) * 4;
                                    if (pixels[nIdx + 3] > 0 && pixels[nIdx] > 240 && 
                                        pixels[nIdx + 1] > 240 && pixels[nIdx + 2] > 240) {
                                        visited[ny * w + nx] = true;
                                        q.push({nx, ny});
                                    }
                                }
                            }
                        }
                        
                        // Size threshold: If it's a small cluster, it's likely a watermark/artifact.
                        if (component.size() > 0 && component.size() < 120) {
                            for (auto& p : component) {
                                mask[p.second * w + p.first] = true;
                            }
                        }
                    }
                }
            }

            // Dilate mask by 1 pixel to catch the anti-aliased edges of the watermark
            std::vector<bool> dilatedMask = mask;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (mask[y * w + x]) {
                        for (int i = 0; i < 8; ++i) {
                            int nx = x + dx[i];
                            int ny = y + dy[i];
                            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                dilatedMask[ny * w + nx] = true;
                            }
                        }
                    }
                }
            }
            mask = dilatedMask;
        }

        // Iterative Mode-Filter Reconstruction (Outside-In)
        bool repairing = true;
        while (repairing) {
            repairing = false;
            std::vector<uint8_t> tempPixels = pixels;
            std::vector<bool> newMask = mask;

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (mask[y * w + x]) {
                        std::map<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>, int> colorCounts;
                        int maxCount = 0;
                        std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> bestColor = {0, 0, 0, 0};
                        bool hasUnmaskedNeighbor = false;

                        // Search local 5x5 neighborhood context
                        for (int dy = -2; dy <= 2; ++dy) {
                            for (int dx = -2; dx <= 2; ++dx) {
                                int nx = x + dx;
                                int ny = y + dy;
                                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                    if (!mask[ny * w + nx]) {
                                        hasUnmaskedNeighbor = true;
                                        int nIdx = (ny * w + nx) * 4;
                                        auto color = std::make_tuple(pixels[nIdx], pixels[nIdx+1], pixels[nIdx+2], pixels[nIdx+3]);
                                        colorCounts[color]++;
                                        if (colorCounts[color] > maxCount) {
                                            maxCount = colorCounts[color];
                                            bestColor = color;
                                        }
                                    }
                                }
                            }
                        }

                        if (hasUnmaskedNeighbor) {
                            int idx = (y * w + x) * 4;
                            tempPixels[idx]     = std::get<0>(bestColor);
                            tempPixels[idx + 1] = std::get<1>(bestColor);
                            tempPixels[idx + 2] = std::get<2>(bestColor);
                            tempPixels[idx + 3] = std::get<3>(bestColor);
                            newMask[y * w + x] = false;
                            repairing = true;
                        }
                    }
                }
            }
            pixels = tempPixels;
            mask = newMask;
        }

        m_newTexture = std::make_shared<SourceTexture>(w, h, std::move(pixels));
        m_executed = true;
    }

    m_project->SetTexture(m_newTexture);
}

void RemoveArtifactsCommand::Undo() {
    if (m_project && m_oldTexture) {
        m_project->SetTexture(m_oldTexture);
    }
}

}