#pragma once
#include <memory>
#include <string>
#include "Processing/SpriteDetector.h"

namespace StudioCore {

class WorkspaceManager;
class Project;
class SourceTexture;
class BackgroundJobQueue;

class StudioEngineFacade {
public:
    StudioEngineFacade();
    ~StudioEngineFacade();

    void Initialize();
    void Update(); 
    void CreateProject();
    bool IsProjectActive() const;
    bool ImportImage(const std::string& filePath, std::string& outErrorMessage);
    
    std::shared_ptr<Project> GetCurrentProject() const;
    std::shared_ptr<const SourceTexture> GetCurrentTexture() const;
    bool HasTexture() const;

    void RunAutoDetection(const DetectionConfig& config);
    void CancelDetection();
    bool IsDetectionRunning() const;
    float GetDetectionProgress() const;

    std::shared_ptr<WorkspaceManager> GetWorkspace() const;

private:
    std::shared_ptr<WorkspaceManager> m_workspace;
    std::unique_ptr<BackgroundJobQueue> m_jobQueue;
};

}