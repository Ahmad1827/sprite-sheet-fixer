#pragma once
#include <memory>

namespace StudioCore {

class WorkspaceManager;

class StudioEngineFacade {
public:
    StudioEngineFacade();
    ~StudioEngineFacade();

    void Initialize();
    void CreateProject();
    bool IsProjectActive() const;

    std::shared_ptr<WorkspaceManager> GetWorkspace() const;

private:
    std::shared_ptr<WorkspaceManager> m_workspace;
};

}