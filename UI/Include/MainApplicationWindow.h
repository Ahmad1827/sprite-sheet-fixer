#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include "StudioEngineFacade.h"
#include "Panels/PreviewViewport.h"
#include "Panels/ExportPreviewPanel.h"
#include "Panels/Toolbar.h"
#include "Panels/AnimationBuilderPanel.h"
#include "Panels/WorkspaceEnvironment.h"

namespace StudioUI {
    class AnimationPanel;
}

class MainApplicationWindow {
public:
    MainApplicationWindow();
    ~MainApplicationWindow();

    void Run();
    bool LoadImage(const std::string& filePath);

private:
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();
    bool m_isDragging{false};
    sf::Vector2f m_dragStartPos{0.f, 0.f};
    std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> m_draggedSprites;
    StudioUI::WorkspaceEnvironment m_workspace;
    float m_autoSaveTimer{0.0f};
    sf::RenderWindow m_window;
    StudioCore::StudioEngineFacade m_engine;
    PreviewViewport m_viewport;
    std::unique_ptr<StudioUI::AnimationPanel> m_animationPanel;
    StudioUI::ExportPreviewPanel m_exportPreview;
    bool m_isExportMode{false};
    bool m_isUIHidden{false};

    StudioUI::AnimationBuilderPanel m_animBuilderPanel;
    bool m_isWizardMode{false};
    StudioUI::Toolbar m_toolbar;
};