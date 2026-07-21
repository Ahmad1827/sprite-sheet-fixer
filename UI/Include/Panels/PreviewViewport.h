#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "Rendering/GridRenderer.h"
#include "Rendering/SelectionGizmo.h"
#include "Rendering/OverlayRenderer.h"
#include "Rendering/CoordinateAxisRenderer.h"

namespace StudioCore {
    class StudioEngineFacade;
}

class PreviewViewport {
public:
    PreviewViewport();
    void Initialize(); // Replaces LoadFont string pass

    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    void Update(float deltaTime);
    void Render(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    
    void RefreshTexture(const StudioCore::StudioEngineFacade& engine);
    void ResetZoom();
    void ResetPan();
    void CenterImage();
    void FitToImage(const sf::RenderWindow& window);
    void ZoomToSelection();
    void CenterOnPoint(const sf::Vector2f& worldPos);

private:
    sf::Vector2i GetMouseImageCoords(const sf::RenderWindow& window) const;
    sf::Color GetPixelColor(const StudioCore::StudioEngineFacade& engine, int x, int y) const;

    sf::Texture m_gpuTexture;
    sf::Sprite m_sprite;
    sf::View m_view;
    
    bool m_hasValidTexture{false};
    float m_currentZoom{1.0f};
    
    // Input state
    bool m_isPanning{false};
    sf::Vector2i m_lastMousePos;
    sf::Clock m_doubleClickTimer;

    // Toggles
    bool m_showGrid{false};
    bool m_showDebug{false};

    // Stats
    float m_fps{0.0f};
    float m_frameTimeMs{0.0f};
    int m_frameCount{0};
    float m_fpsTimer{0.0f};

    // Renderer Instances
    StudioUI::GridRenderer m_grid;
    StudioUI::SelectionGizmo m_selection;
    StudioUI::OverlayRenderer m_overlay;
    StudioUI::CoordinateAxisRenderer m_axes;
};