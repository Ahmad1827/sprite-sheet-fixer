#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"
#include "Systems/BackgroundJobQueue.h"
#include "Systems/PlaybackEngine.h"
#include "Systems/ExportManager.h"
#include "Systems/ProjectManager.h"
#include "Commands/CommandHistory.h"
#include "Commands/EditMetadataCommand.h"
#include "Commands/AnimationCommands.h"
#include "Processing/ImageLoader.h"
#include "DataModels/Project.h"
#include "Commands/BatchCommands.h"
#include "Commands/PixelCommands.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/SourceTexture.h"
#include <iostream>
#include "Commands/MoveSpriteCommand.h"
#include "Commands/MoveSpriteWithPixelsCommand.h"
#include "Commands/RepackFramesCommand.h"
#include "Commands/FlipHorizontalCommand.h"
#include "Commands/MergeSpritesCommand.h"
#include "Commands/RemoveArtifactsCommand.h"
#include <cmath>
#include <queue>

namespace StudioCore {

StudioEngineFacade::StudioEngineFacade() = default;
StudioEngineFacade::~StudioEngineFacade() = default;

void StudioEngineFacade::Initialize() {
    m_workspace = std::make_shared<WorkspaceManager>();
    m_jobQueue = std::make_unique<BackgroundJobQueue>();
    m_commandHistory = std::make_unique<CommandHistory>();
    m_playbackEngine = std::make_unique<PlaybackEngine>();
    m_exportManager = std::make_unique<ExportManager>();
}

void StudioEngineFacade::Update(float deltaTime) {
    if (m_jobQueue && m_jobQueue->HasResults()) {
        auto results = m_jobQueue->ConsumeResults();
        if (IsProjectActive()) {
            for (auto& sprite : results) {
                m_workspace->GetActiveProject()->AddSprite(*sprite); // <-- Added dereference here
            }
        }
    }
    if (IsProjectActive() && m_playbackEngine) {
        m_playbackEngine->Update(deltaTime, m_workspace->GetActiveProject().get());
    }
}

void StudioEngineFacade::CreateProject() {
    if (m_workspace) m_workspace->CreateNewProject();
    if (m_commandHistory) m_commandHistory->Clear();
    if (m_playbackEngine) m_playbackEngine->Stop();
    m_animIdCounter = 1;
}

bool StudioEngineFacade::IsProjectActive() const {
    return m_workspace && m_workspace->HasActiveProject();
}

bool StudioEngineFacade::ImportImage(const std::string& filePath, std::string& outErrorMessage) {
    if (!IsProjectActive()) return false;
    auto texture = ImageLoader::LoadFromFile(filePath, outErrorMessage);
    if (!texture) return false;
    m_workspace->GetActiveProject()->SetTexture(std::move(texture));
    m_workspace->GetActiveProject()->SetImagePath(filePath);
    return true;
}

bool StudioEngineFacade::SaveProject(const std::string& filePath) const {
    if (!IsProjectActive()) return false;
    return ProjectManager::SaveProject(*GetCurrentProject(), filePath);
}

bool StudioEngineFacade::LoadProject(const std::string& filePath, std::string& outErrorMessage) {
    auto proj = ProjectManager::LoadProject(filePath, outErrorMessage);
    if (!proj) return false;

    // Use std::const_pointer_cast to pass the loaded texture to SetTexture cleanly
    m_workspace->CreateNewProject();
    auto tex = std::const_pointer_cast<SourceTexture>(proj->GetTexture());
    m_workspace->GetActiveProject()->SetTexture(tex);
    m_workspace->GetActiveProject()->SetImagePath(proj->GetImagePath());

    for (const auto& s : proj->GetSprites()) {
        m_workspace->GetActiveProject()->AddSprite(*s);
    }
    for (const auto& a : proj->GetAnimationGroups()) {
        m_workspace->GetActiveProject()->AddAnimationGroup(std::make_shared<AnimationGroup>(*a));
    }

    if (m_commandHistory) m_commandHistory->Clear();
    if (m_playbackEngine) m_playbackEngine->Stop();

    return true;
}

std::shared_ptr<Project> StudioEngineFacade::GetCurrentProject() const {
    if (IsProjectActive()) return m_workspace->GetActiveProject();
    return nullptr;
}

std::shared_ptr<const SourceTexture> StudioEngineFacade::GetCurrentTexture() const {
    auto p = GetCurrentProject();
    if (p) return p->GetTexture();
    return nullptr;
}

bool StudioEngineFacade::HasTexture() const {
    return GetCurrentTexture() != nullptr;
}

void StudioEngineFacade::RunAutoDetection(const DetectionConfig& config) {
    if (!HasTexture() || m_jobQueue->IsRunning()) return;
    std::shared_ptr<const SourceTexture> tex = GetCurrentTexture();
    m_jobQueue->StartJob([tex, config](std::atomic<float>& p, std::atomic<bool>& c) {
        return SpriteDetector::Detect(*tex, config, p, c);
    });
}

void StudioEngineFacade::CancelDetection() {
    if (m_jobQueue) m_jobQueue->Cancel();
}

bool StudioEngineFacade::IsDetectionRunning() const {
    return m_jobQueue && m_jobQueue->IsRunning();
}

float StudioEngineFacade::GetDetectionProgress() const {
    return m_jobQueue ? m_jobQueue->GetProgress() : 0.0f;
}

void StudioEngineFacade::Undo() {
    if (m_commandHistory) {
        m_commandHistory->Undo();
    }
}

void StudioEngineFacade::Redo() {
    if (m_commandHistory) {
        m_commandHistory->Redo();
    }
}

void StudioEngineFacade::EditPivot(const std::vector<std::string>& spriteIds, Point newPivot) {
    if (!IsProjectActive() || spriteIds.empty()) return;
    auto cmd = std::make_unique<EditMetadataCommand>(GetCurrentProject(), spriteIds, EditType::Pivot, newPivot, 0.0f);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::EditBaseline(const std::vector<std::string>& spriteIds, float newBaseline) {
    if (!IsProjectActive() || spriteIds.empty()) return;
    auto cmd = std::make_unique<EditMetadataCommand>(GetCurrentProject(), spriteIds, EditType::Baseline, Point{0,0}, newBaseline);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::CreateAnimation(const std::string& name) {
    if (!IsProjectActive()) return;
    std::string id = "anim_" + std::to_string(m_animIdCounter++);
    auto cmd = std::make_unique<CreateAnimationCommand>(GetCurrentProject(), id, name);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::DeleteAnimation(const std::string& id) {
    if (!IsProjectActive()) return;
    if (m_playbackEngine->GetActiveAnimation() == id) m_playbackEngine->Stop();
    auto cmd = std::make_unique<DeleteAnimationCommand>(GetCurrentProject(), id);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::ModifyAnimationFrames(const std::string& id, const std::vector<std::string>& newFrames) {
    if (!IsProjectActive()) return;
    auto cmd = std::make_unique<ModifyFramesCommand>(GetCurrentProject(), id, newFrames);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::EditAnimationSettings(const std::string& id, const std::string& newName, float fps, bool looping) {
    if (!IsProjectActive()) return;
    auto cmd = std::make_unique<EditAnimationSettingsCommand>(GetCurrentProject(), id, newName, fps, looping);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::ToggleAutoAlign() {
    if (m_playbackEngine) {
        m_playbackEngine->SetAutoAlign(!m_playbackEngine->IsAutoAlignEnabled());
    }
}

bool StudioEngineFacade::IsAutoAlignEnabled() const {
    return m_playbackEngine ? m_playbackEngine->IsAutoAlignEnabled() : false;
}

sf::Image StudioEngineFacade::GenerateExportPreview(int padding, bool keepOriginalResolution) const {
    if (!IsProjectActive()) return sf::Image();

    auto tex = GetCurrentTexture();
    if (!tex || !tex->IsValid()) return sf::Image();

    int w = tex->GetWidth();
    int h = tex->GetHeight();
    const auto& pixels = tex->GetPixels();

    if (keepOriginalResolution) {
        sf::Image img;
        img.create(w, h, pixels.data());
        return img;
    }

    if (m_exportManager) {
        sf::Image mgrPreview = m_exportManager->GeneratePreview(*GetCurrentProject(), padding);
        if (mgrPreview.getSize().x > 0 && mgrPreview.getSize().y > 0) {
            return mgrPreview;
        }
    }

    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = (y * w + x) * 4;
            if (pixels[idx + 3] > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        sf::Image emptyImg;
        emptyImg.create(1, 1, sf::Color::Transparent);
        return emptyImg;
    }

    int cropW = (maxX - minX) + 1;
    int cropH = (maxY - minY) + 1;

    sf::Image croppedImg;
    croppedImg.create(cropW, cropH);

    for (int cy = 0; cy < cropH; ++cy) {
        for (int cx = 0; cx < cropW; ++cx) {
            size_t srcIdx = ((minY + cy) * w + (minX + cx)) * 4;
            croppedImg.setPixel(cx, cy, sf::Color(
                pixels[srcIdx],
                pixels[srcIdx + 1],
                pixels[srcIdx + 2],
                pixels[srcIdx + 3]
            ));
        }
    }

    return croppedImg;
}

bool StudioEngineFacade::ExportPNG(const std::string& filePath, int padding, bool keepOriginalResolution) const {
    sf::Image img = GenerateExportPreview(padding, keepOriginalResolution);
    if (img.getSize().x == 0 || img.getSize().y == 0) return false;
    return img.saveToFile(filePath);
}

PlaybackEngine& StudioEngineFacade::GetPlaybackEngine() {
    return *m_playbackEngine;
}

const PlaybackEngine& StudioEngineFacade::GetPlaybackEngine() const {
    return *m_playbackEngine;
}

std::shared_ptr<WorkspaceManager> StudioEngineFacade::GetWorkspace() const {
    return m_workspace;
}

void StudioEngineFacade::ExecuteBatchOperation(const std::vector<std::string>& spriteIds, BatchOp op) {
    if (!IsProjectActive() || spriteIds.empty()) return;
    auto cmd = std::make_unique<BatchOperationCommand>(GetCurrentProject(), spriteIds, op);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::ExecuteAlignSprites(const std::vector<std::string>& spriteIds, AlignOp op) {
    if (!IsProjectActive() || spriteIds.empty()) return;
    auto cmd = std::make_unique<AlignSpritesCommand>(GetCurrentProject(), spriteIds, op);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

// Implementation for Facade:
std::vector<ProposedAnimation> StudioEngineFacade::BuildAnimationsByRow() {
    auto proj = GetCurrentProject();
    if (!proj) return {};
    
    AnimationBuilder builder;
    return builder.DetectByRows(proj->GetSprites());
}

void StudioEngineFacade::CommitProposedAnimations(const std::vector<ProposedAnimation>& animations) {
    auto proj = GetCurrentProject();
    if (!proj) return;

    for (const auto& prop : animations) {
        std::string animId = "anim_" + prop.name;
        
        auto group = std::make_shared<AnimationGroup>(animId, prop.name);
        group->SetFPS(static_cast<float>(prop.fps));
        group->SetLooping(prop.isLooping);
        
        auto idsToAdd = prop.spriteIds;
        if (prop.reverseOrder) {
            std::reverse(idsToAdd.begin(), idsToAdd.end());
        }

        // Set all frames at once using the built-in method
        group->SetFrames(idsToAdd);

        proj->AddAnimationGroup(group);
    }
}



void StudioEngineFacade::DeleteSpriteWithPixels(const std::string& spriteId) {
    auto proj = GetCurrentProject();
    if (!proj) return;

    auto constTexture = proj->GetTexture();
    if (!constTexture) return;

    auto texture = std::const_pointer_cast<SourceTexture>(constTexture);
    if (!texture || !texture->IsValid()) return;

    auto sprite = proj->GetSpriteById(spriteId);
    if (!sprite) return;

    auto rect = sprite->GetSourceRect();
    int imgWidth = texture->GetWidth();
    int imgHeight = texture->GetHeight();

    sf::Image currentCanvas;
    currentCanvas.create(imgWidth, imgHeight, texture->GetPixels().data());
    sf::Image erasedCanvas = currentCanvas;

    sf::IntRect srcRect(static_cast<int>(rect.x), static_cast<int>(rect.y),
                        static_cast<int>(rect.width), static_cast<int>(rect.height));

    for (int y = srcRect.top; y < srcRect.top + srcRect.height; ++y) {
        for (int x = srcRect.left; x < srcRect.left + srcRect.width; ++x) {
            if (x >= 0 && x < imgWidth && y >= 0 && y < imgHeight) {
                erasedCanvas.setPixel(x, y, sf::Color::Transparent);
            }
        }
    }

    std::vector<uint8_t> oldPixels = texture->GetPixels();
    std::vector<uint8_t> newPixels(erasedCanvas.getPixelsPtr(), erasedCanvas.getPixelsPtr() + (imgWidth * imgHeight * 4));

    auto pixelCmd = std::make_unique<PixelRegionCommand>(texture, oldPixels, newPixels);
    pixelCmd->Execute();

    proj->RemoveSprite(spriteId);

    if (m_commandHistory) {
        m_commandHistory->ExecuteCommand(std::move(pixelCmd));
    }
}

void StudioEngineFacade::RepackFrames() {
    auto project = GetCurrentProject();
    if (!IsProjectActive() || !project) return;
    
    // Auto-detect if the user forgot
    if (project->GetSprites().empty()) {
        DetectionConfig config;
        config.minSpriteSize = 25; // <--- ADD THIS LINE! Ignores tiny junk boxes.
        RunAutoDetection(config);
    }
    
    if (project->GetSprites().empty()) return; 

    auto cmd = std::make_unique<RepackFramesCommand>(project);
    m_commandHistory->ExecuteCommand(std::move(cmd)); 
}

void StudioEngineFacade::FlipHorizontal() {
    auto project = GetCurrentProject();
    if (!IsProjectActive() || !project) return;
    
    if (project->GetSprites().empty()) {
        DetectionConfig config;
        config.minSpriteSize = 35;
        RunAutoDetection(config);
    }
    
    if (project->GetSprites().empty()) return;

    auto cmd = std::make_unique<FlipHorizontalCommand>(project);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::MergeOverlappingSprites() {
    auto project = GetCurrentProject();
    if (!IsProjectActive() || !project || project->GetSprites().empty()) return;
    
    auto cmd = std::make_unique<MergeSpritesCommand>(project);
    m_commandHistory->ExecuteCommand(std::move(cmd));
}

void StudioEngineFacade::RemoveArtifacts(int targetX, int targetY) {
    auto project = GetCurrentProject();
    if (!project) return;

    auto currentTex = project->GetTexture();
    if (!currentTex) return;

    int texWidth = currentTex->GetWidth();
    int texHeight = currentTex->GetHeight();

    if (targetX < 0 || targetX >= texWidth || targetY < 0 || targetY >= texHeight) return;

    auto* mutableTex = const_cast<SourceTexture*>(currentTex.get());
    auto& pixels = mutableTex->GetPixelsMutable();

    size_t targetIdx = (static_cast<size_t>(targetY) * texWidth + targetX) * 4;
    uint8_t targetR = pixels[targetIdx];
    uint8_t targetG = pixels[targetIdx + 1];
    uint8_t targetB = pixels[targetIdx + 2];
    uint8_t targetA = pixels[targetIdx + 3];

    if (targetA == 0) return;

    float tolerance = 25.0f;
    std::vector<bool> visited(texWidth * texHeight, false);
    std::queue<std::pair<int, int>> q;

    q.push({targetX, targetY});
    visited[targetY * texWidth + targetX] = true;

    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        size_t idx = (static_cast<size_t>(cy) * texWidth + cx) * 4;
        pixels[idx] = 0;
        pixels[idx + 1] = 0;
        pixels[idx + 2] = 0;
        pixels[idx + 3] = 0;

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < texWidth && ny >= 0 && ny < texHeight) {
                int nPos = ny * texWidth + nx;
                if (!visited[nPos]) {
                    visited[nPos] = true;
                    size_t nIdx = static_cast<size_t>(nPos) * 4;
                    if (pixels[nIdx + 3] > 0) {
                        float dr = static_cast<float>(pixels[nIdx]) - targetR;
                        float dg = static_cast<float>(pixels[nIdx + 1]) - targetG;
                        float db = static_cast<float>(pixels[nIdx + 2]) - targetB;
                        if (std::sqrt(dr * dr + dg * dg + db * db) <= tolerance) {
                            q.push({nx, ny});
                        }
                    }
                }
            }
        }
    }
}

void StudioEngineFacade::RemoveArtifactsArea(int x, int y, int width, int height) {
    auto project = GetCurrentProject();
    if (!project) return;

    auto currentTex = project->GetTexture();
    if (!currentTex) return;

    int texWidth = currentTex->GetWidth();
    int texHeight = currentTex->GetHeight();

    int startX = std::max(0, x);
    int startY = std::max(0, y);
    int endX = std::min(texWidth, x + width);
    int endY = std::min(texHeight, y + height);

    if (startX >= endX || startY >= endY) return;

    auto* mutableTex = const_cast<SourceTexture*>(currentTex.get());
    auto& pixels = mutableTex->GetPixelsMutable();

    size_t sampleIdx = (static_cast<size_t>(startY) * texWidth + startX) * 4;
    uint8_t sampleR = pixels[sampleIdx];
    uint8_t sampleG = pixels[sampleIdx + 1];
    uint8_t sampleB = pixels[sampleIdx + 2];
    uint8_t sampleA = pixels[sampleIdx + 3];

    float tolerance = 30.0f;

    for (int py = startY; py < endY; ++py) {
        for (int px = startX; px < endX; ++px) {
            size_t idx = (static_cast<size_t>(py) * texWidth + px) * 4;
            if (pixels[idx + 3] > 0) {
                if (sampleA > 0) {
                    float dr = static_cast<float>(pixels[idx]) - sampleR;
                    float dg = static_cast<float>(pixels[idx + 1]) - sampleG;
                    float db = static_cast<float>(pixels[idx + 2]) - sampleB;
                    if (std::sqrt(dr * dr + dg * dg + db * db) <= tolerance) {
                        pixels[idx] = 0;
                        pixels[idx + 1] = 0;
                        pixels[idx + 2] = 0;
                        pixels[idx + 3] = 0;
                    }
                } else {
                    pixels[idx] = 0;
                    pixels[idx + 1] = 0;
                    pixels[idx + 2] = 0;
                    pixels[idx + 3] = 0;
                }
            }
        }
    }
}

void StudioEngineFacade::RemoveColorGlobal(int targetX, int targetY, float tolerance) {
    auto project = GetCurrentProject();
    if (!project) return;
    
    auto currentTex = project->GetTexture();
    if (!currentTex) return;

    int texWidth = currentTex->GetWidth();
    int texHeight = currentTex->GetHeight();

    if (targetX < 0 || targetX >= texWidth || targetY < 0 || targetY >= texHeight) return;

    auto* mutableTex = const_cast<SourceTexture*>(currentTex.get());
    auto& pixels = mutableTex->GetPixelsMutable();

    size_t targetIdx = (static_cast<size_t>(targetY) * texWidth + targetX) * 4;
    uint8_t tr = pixels[targetIdx];
    uint8_t tg = pixels[targetIdx + 1];
    uint8_t tb = pixels[targetIdx + 2];
    uint8_t ta = pixels[targetIdx + 3];

    if (ta == 0) return;

    for (size_t i = 0; i < static_cast<size_t>(texWidth) * texHeight; ++i) {
        size_t idx = i * 4;
        if (pixels[idx + 3] == 0) continue;

        float dr = static_cast<float>(pixels[idx]) - tr;
        float dg = static_cast<float>(pixels[idx + 1]) - tg;
        float db = static_cast<float>(pixels[idx + 2]) - tb;
        float dist = std::sqrt(dr * dr + dg * dg + db * db);

        if (dist <= tolerance) {
            pixels[idx] = 0;
            pixels[idx + 1] = 0;
            pixels[idx + 2] = 0;
            pixels[idx + 3] = 0;
        }
    }
}
}