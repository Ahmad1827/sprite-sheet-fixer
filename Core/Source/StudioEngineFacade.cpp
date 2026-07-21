#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"

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

std::shared_ptr<WorkspaceManager> StudioEngineFacade::GetWorkspace() const {
    return m_workspace;
}

}