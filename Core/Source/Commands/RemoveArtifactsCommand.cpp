#include "Commands/RemoveArtifactsCommand.h"
#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include <queue>
#include <map>
#include <tuple>
#include <algorithm>

namespace StudioCore {

RemoveArtifactsCommand::RemoveArtifactsCommand(std::shared_ptr<Project> project, int startX, int startY)
    : m_project(project), m_startX(startX), m_startY(startY) {}

void RemoveArtifactsCommand::Execute() {
    if (!m_executed) {
        m_oldTexture = std::const_pointer_cast<SourceTexture>(m_project->GetTexture());
        if (!m_oldTexture) return;

        int w = m_oldTexture->GetWidth();
        int h = m_oldTexture->GetHeight();
        std::vector<uint8_t> pixels = m_oldTexture->GetPixels();
        std::vector<bool> mask(w * h, false);

        int radius = 15;
        for (int y = std::max(0, m_startY - radius); y <= std::min(h - 1, m_startY + radius); ++y) {
            for (int x = std::max(0, m_startX - radius); x <= std::min(w - 1, m_startX + radius); ++x) {
                int idx = (y * w + x) * 4;
                if (pixels[idx + 3] > 0 && pixels[idx] > 200 && pixels[idx + 1] > 200 && pixels[idx + 2] > 200) {
                    mask[y * w + x] = true;
                }
            }
        }

        std::vector<bool> dilatedMask = mask;
        const int dx[] = {1, 1, 1, 0, -1, -1, -1, 0};
        const int dy[] = {-1, 0, 1, 1, 1, 0, -1, -1};
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

                        for (int dy2 = -2; dy2 <= 2; ++dy2) {
                            for (int dx2 = -2; dx2 <= 2; ++dx2) {
                                int nx = x + dx2;
                                int ny = y + dy2;
                                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                    if (!mask[ny * w + nx]) {
                                        hasUnmaskedNeighbor = true;
                                        int nIdx = (ny * w + nx) * 4;
                                        auto color = std::make_tuple(pixels[nIdx], pixels[nIdx+1], pixels[nIdx+2], pixels[nIdx+3]);
                                        if (std::get<3>(color) > 0) {
                                            colorCounts[color]++;
                                            if (colorCounts[color] > maxCount) {
                                                maxCount = colorCounts[color];
                                                bestColor = color;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (hasUnmaskedNeighbor && maxCount > 0) {
                            int idx = (y * w + x) * 4;
                            tempPixels[idx]     = std::get<0>(bestColor);
                            tempPixels[idx + 1] = std::get<1>(bestColor);
                            tempPixels[idx + 2] = std::get<2>(bestColor);
                            tempPixels[idx + 3] = std::get<3>(bestColor);
                            newMask[y * w + x] = false;
                            repairing = true;
                        } else if (hasUnmaskedNeighbor) {
                            int idx = (y * w + x) * 4;
                            tempPixels[idx + 3] = 0;
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