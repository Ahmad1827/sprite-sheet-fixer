#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include "StudioEngineFacade.h"
#include "Panels/PreviewViewport.h"
#include "Panels/Toolbar.h"
#include "Panels/WorkspaceEnvironment.h"
#include "Panels/AnimationPanel.h"
#include "Panels/ExportPreviewPanel.h"
#include "Panels/AnimationBuilderPanel.h"

namespace StudioUI {

class SpriteSheetStudioPanel {
public:
    SpriteSheetStudioPanel();
    ~SpriteSheetStudioPanel();

    void Initialize();
    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void Update(float deltaTime, const sf::RenderWindow& window);
    void Render(sf::RenderWindow& window);
    void SetBounds(const sf::FloatRect& bounds);

    void LoadImage(const std::string& filePath);

private:
    StudioCore::StudioEngineFacade m_engine;
    PreviewViewport m_viewport;
    Toolbar m_toolbar;
    WorkspaceEnvironment m_workspace;
    std::unique_ptr<AnimationPanel> m_animationPanel;
    ExportPreviewPanel m_exportPreview;
    AnimationBuilderPanel m_animBuilderPanel;

    sf::FloatRect m_bounds{0.f, 0.f, 1280.f, 720.f};
    bool m_isActive = true;
    bool m_isUIHidden = false;
    bool m_isWizardMode = false;
    bool m_isExportMode = false;

    bool m_isArtifactMode = false;
    bool m_isInfillMode = false;
    bool m_isDeleteMode = false;
    bool m_isDraggingArtifact = false;

    sf::Vector2f m_artifactDragStart{0.f, 0.f};
    sf::Vector2f m_artifactDragCurrent{0.f, 0.f};
    sf::Vector2i m_dragStartPixel{0, 0};
    sf::Vector2i m_dragCurrentPixel{0, 0};
};

}