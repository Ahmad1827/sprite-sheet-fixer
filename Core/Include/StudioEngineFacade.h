#pragma once
#include <memory>
#include <string>

namespace StudioCore {

class WorkspaceManager;
class Project;
class SourceTexture;

class StudioEngineFacade {
public:
    StudioEngineFacade();
    ~StudioEngineFacade();

    void Initialize();
    void CreateProject();
    bool IsProjectActive() const;

    // Imports a PNG/image file into the currently active project
    bool ImportImage(const std::string& filePath, std::string& outErrorMessage);

    std::shared_ptr<Project> GetCurrentProject() const;
    std::shared_ptr<const SourceTexture> GetCurrentTexture() const;
    bool HasTexture() const;

    std::shared_ptr<WorkspaceManager> GetWorkspace() const;

private:
    std::shared_ptr<WorkspaceManager> m_workspace;
};

}