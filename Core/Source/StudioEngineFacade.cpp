#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"
#include "Systems/BackgroundJobQueue.h"
#include "Systems/PlaybackEngine.h"
#include "Commands/CommandHistory.h"
#include "Commands/EditMetadataCommand.h"
#include "Commands/AnimationCommands.h"
#include "Processing/ImageLoader.h"
#include "DataModels/Project.h"

namespace StudioCore {

StudioEngineFacade::StudioEngineFacade() = default;
StudioEngineFacade::~StudioEngineFacade() = default;

void StudioEngineFacade::Initialize() {
    m_workspace = std::make_shared<WorkspaceManager>();
    m_jobQueue = std::make_unique<BackgroundJobQueue>();
    m_commandHistory = std::make_unique<CommandHistory>();
    m_playbackEngine = std::make_unique<PlaybackEngine>();
}

void StudioEngineFacade::Update(float deltaTime) {
    if (m_jobQueue && m_jobQueue->HasResults()) {
        auto results = m_jobQueue->ConsumeResults();
        if (IsProjectActive()) {
            for (auto& sprite : results) {
                m_workspace->GetActiveProject()->AddSprite(sprite);
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
    if (m_commandHistory) m_commandHistory->Undo();
}

void StudioEngineFacade::Redo() {
    if (m_commandHistory) m_commandHistory->Redo();
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

PlaybackEngine& StudioEngineFacade::GetPlaybackEngine() {
    return *m_playbackEngine;
}

const PlaybackEngine& StudioEngineFacade::GetPlaybackEngine() const {
    return *m_playbackEngine;
}

std::shared_ptr<WorkspaceManager> StudioEngineFacade::GetWorkspace() const {
    return m_workspace;
}

}