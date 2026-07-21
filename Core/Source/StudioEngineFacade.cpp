#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"
#include "Systems/BackgroundJobQueue.h"
#include "Processing/ImageLoader.h"
#include "DataModels/Project.h"

namespace StudioCore {

StudioEngineFacade::StudioEngineFacade() = default;
StudioEngineFacade::~StudioEngineFacade() = default;

void StudioEngineFacade::Initialize() {
    m_workspace = std::make_shared<WorkspaceManager>();
    m_jobQueue = std::make_unique<BackgroundJobQueue>();
}

void StudioEngineFacade::Update() {
    if (m_jobQueue && m_jobQueue->HasResults()) {
        auto results = m_jobQueue->ConsumeResults();
        if (IsProjectActive()) {
            for (auto& sprite : results) {
                m_workspace->GetActiveProject()->AddSprite(sprite);
            }
        }
    }
}

void StudioEngineFacade::CreateProject() {
    if (m_workspace) m_workspace->CreateNewProject();
}

bool StudioEngineFacade::IsProjectActive() const {
    return m_workspace && m_workspace->HasActiveProject();
}

bool StudioEngineFacade::ImportImage(const std::string& filePath, std::string& outErrorMessage) {
    if (!IsProjectActive()) return false;
    auto texture = ImageLoader::LoadFromFile(filePath, outErrorMessage);
    if (!texture) return false;
    m_workspace->GetActiveProject()->SetTexture(std::move(texture));
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

std::shared_ptr<WorkspaceManager> StudioEngineFacade::GetWorkspace() const {
    return m_workspace;
}

}