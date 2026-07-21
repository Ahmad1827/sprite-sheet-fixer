#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>
#include "Rendering/GridRenderer.h"
#include "Rendering/SelectionGizmo.h"
#include "Rendering/OverlayRenderer.h"
#include "Rendering/CoordinateAxisRenderer.h"
#include "Rendering/SpriteGizmoRenderer.h"

namespace StudioCore {
    class StudioEngineFacade;
}

class PreviewViewport {
public:
    PreviewViewport();
    void Initialize();
    const std::vector<std::string>& GetSelectedSpriteIds() const { return m_selectedSpriteIds; }
    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine);
    void Update(float deltaTime);
    void Render(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    
    void ClearSelection() { 
        m_selectedSpriteIds.clear(); 
        m_selection.ClearSelection();
    }

    sf::FloatRect GetViewportBounds(const sf::RenderWindow& window) const {
        sf::Vector2u size = window.getSize();
        return sf::FloatRect(0.f, 0.f, static_cast<float>(size.x) - 300.f, static_cast<float>(size.y) - 200.f);
    }
    
    void RefreshTexture(const StudioCore::StudioEngineFacade& engine);
    void ResetZoom();
    void ResetPan();
    void CenterImage();
    void FitToImage(const sf::RenderWindow& window);
    void ZoomToSelection();
    void CenterOnPoint(const sf::Vector2f& worldPos);
    void TriggerNumericEdit(StudioCore::StudioEngineFacade& engine);

private:
    sf::Vector2i GetMouseImageCoords(const sf::RenderWindow& window) const;
    sf::Color GetPixelColor(const StudioCore::StudioEngineFacade& engine, int x, int y) const;
    void HandleSpriteSelection(const sf::Vector2f& worldPos, const StudioCore::StudioEngineFacade& engine, bool shift, bool ctrl);

    sf::Texture m_gpuTexture;
    sf::Sprite m_sprite;
    sf::View m_view;
    
    bool m_hasValidTexture{false};
    float m_currentZoom{1.0f};
    
    bool m_isPanning{false};
    bool m_isDraggingPivot{false};
    bool m_isDraggingBaseline{false};
    sf::Vector2i m_lastMousePos;
    sf::Clock m_doubleClickTimer;

    bool m_showGrid{false};
    bool m_showDebug{false};
    bool m_showBoxes{true};
    bool m_showCenters{true};
    bool m_showIds{true};
    bool m_showPivots{true};
    bool m_showBaselines{true};

    std::vector<std::string> m_selectedSpriteIds;
    std::string m_hoveredSpriteId;

    float m_fps{0.0f};
    float m_frameTimeMs{0.0f};
    int m_frameCount{0};
    float m_fpsTimer{0.0f};

    StudioUI::GridRenderer m_grid;
    StudioUI::SelectionGizmo m_selection;
    StudioUI::OverlayRenderer m_overlay;
    StudioUI::CoordinateAxisRenderer m_axes;
    StudioUI::SpriteGizmoRenderer m_spriteGizmos;
};