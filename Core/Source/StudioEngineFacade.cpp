#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"
#include "Processing/ImageLoader.h"
#include "DataModels/Project.h"

namespace StudioCore {

StudioEngineFacade::StudioEngineFacade() = default;
StudioEngineFacade::~StudioEngineFacade() = default;

void StudioEngineFacade::Initialize() {
    m_workspace = std::make_shared<WorkspaceManager>();
}

void StudioEngineFacade::CreateProject() {
    if (m_workspace) {
        m_workspace->CreateNewProject();
    }
}

bool StudioEngineFacade::IsProjectActive() const {
    return m_workspace && m_workspace->HasActiveProject();
}

bool StudioEngineFacade::ImportImage(const std::string& filePath, std::string& outErrorMessage) {
    if (!IsProjectActive()) {
        outErrorMessage = "Cannot import image: No active project.";
        return false;
    }

    auto texture = ImageLoader::LoadFromFile(filePath, outErrorMessage);
    if (!texture) {
        return false;
    }

    m_workspace->GetActiveProject()->SetTexture(std::move(texture));
    return true;
}

std::shared_ptr<Project> StudioEngineFacade::GetCurrentProject() const {
    if (IsProjectActive()) {
        return m_workspace->GetActiveProject();
    }
    return nullptr;
}

std::shared_ptr<const SourceTexture> StudioEngineFacade::GetCurrentTexture() const {
    auto project = GetCurrentProject();
    if (project) {
        return project->GetTexture();
    }
    return nullptr;
}

bool StudioEngineFacade::HasTexture() const {
    return GetCurrentTexture() != nullptr;
}

std::shared_ptr<WorkspaceManager> StudioEngineFacade::GetWorkspace() const {
    return m_workspace;
}

}