#include "Rendering/SpriteGizmoRenderer.h"
#include "DataModels/SpriteDefinition.h"

namespace StudioUI {

SpriteGizmoRenderer::SpriteGizmoRenderer() {
    m_box.setFillColor(sf::Color::Transparent);
    
    m_centerDot.setFillColor(sf::Color::Red);
    m_centerDot.setOutlineColor(sf::Color::White);
    m_centerDot.setOutlineThickness(1.0f);
}

void SpriteGizmoRenderer::SetHoveredSprite(const std::string& id) {
    m_hoveredId = id;
}

void SpriteGizmoRenderer::SetSelectedSprite(const std::string& id) {
    m_selectedId = id;
}

void SpriteGizmoRenderer::Render(sf::RenderWindow& window, 
                                 const std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>& sprites, 
                                 float currentZoom,
                                 bool showBoxes,
                                 bool showCenters,
                                 bool showIds,
                                 sf::Font* font) {
    
    float invZoom = std::max(1.0f, 1.0f / currentZoom);

    for (const auto& sprite : sprites) {
        const auto& rect = sprite->GetSourceRect();
        
        bool isHovered = (sprite->GetId() == m_hoveredId);
        bool isSelected = (sprite->GetId() == m_selectedId);

        if (showBoxes || isHovered || isSelected) {
            m_box.setPosition(rect.x, rect.y);
            m_box.setSize(sf::Vector2f(rect.width, rect.height));
            
            if (isSelected) {
                m_box.setOutlineColor(sf::Color::Yellow);
                m_box.setOutlineThickness(2.0f * invZoom);
            } else if (isHovered) {
                m_box.setOutlineColor(sf::Color::Cyan);
                m_box.setOutlineThickness(1.5f * invZoom);
            } else {
                m_box.setOutlineColor(sf::Color(255, 0, 255, 150));
                m_box.setOutlineThickness(1.0f * invZoom);
            }
            window.draw(m_box);
        }

        if (showCenters) {
            const auto& c = sprite->GetCenter();
            m_centerDot.setRadius(2.0f * invZoom);
            m_centerDot.setOrigin(m_centerDot.getRadius(), m_centerDot.getRadius());
            m_centerDot.setPosition(c.x, c.y);
            window.draw(m_centerDot);
        }

        if (showIds && font) {
            sf::Text idText(sprite->GetId(), *font, static_cast<unsigned int>(12 * invZoom));
            idText.setPosition(rect.x, rect.y - 14 * invZoom);
            idText.setFillColor(sf::Color::Yellow);
            window.draw(idText);
        }
    }
}

}