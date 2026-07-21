#pragma once
#include <SFML/Graphics.hpp>
#include <string>

namespace StudioUI {

struct DebugInfo {
    float fps{0.0f};
    float frameTimeMs{0.0f};
    float zoomLevel{1.0f};
    sf::Vector2f panOffset;
    sf::Vector2f viewportSize;
    int imageWidth{0};
    int imageHeight{0};
    bool projectLoaded{false};
    bool textureLoaded{false};
    float selectionWidth{0.0f};
    float selectionHeight{0.0f};
};

struct InspectorInfo {
    sf::Vector2i mouseWindow;
    sf::Vector2i mouseImage;
    sf::Color pixelColor;
    float currentZoom{1.0f};
    sf::Vector2f panOffset;
    int imageWidth{0};
    int imageHeight{0};
    bool isHoveringImage{false};
};

class OverlayRenderer {
public:
    OverlayRenderer();
    
    // Robust font loading with OS fallbacks
    bool InitializeFont(const std::string& customPath);

    void RenderDebug(sf::RenderWindow& window, const DebugInfo& info);
    void RenderInspector(sf::RenderWindow& window, const InspectorInfo& info);

private:
    sf::Font m_font;
    bool m_hasFont{false};
    void DrawTextShadowed(sf::RenderWindow& window, const std::string& str, const sf::Vector2f& pos);
};

}