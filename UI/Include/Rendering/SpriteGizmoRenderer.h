#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>

namespace StudioCore {
    class SpriteDefinition;
}

namespace StudioUI {

class SpriteGizmoRenderer {
public:
    SpriteGizmoRenderer();
    
    void SetHoveredSprite(const std::string& id);
    void SetSelectedSprite(const std::string& id);
    
    void Render(sf::RenderWindow& window, 
                const std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>& sprites, 
                float currentZoom,
                bool showBoxes,
                bool showCenters,
                bool showIds,
                sf::Font* font);

private:
    std::string m_hoveredId;
    std::string m_selectedId;
    
    sf::RectangleShape m_box;
    sf::CircleShape m_centerDot;
};

}