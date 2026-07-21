#include "Rendering/SpriteGizmoRenderer.h"
#include "DataModels/SpriteDefinition.h"
#include <algorithm>

namespace StudioUI {

SpriteGizmoRenderer::SpriteGizmoRenderer() {
    m_box.setFillColor(sf::Color::Transparent);
    
    m_centerDot.setFillColor(sf::Color::Red);
    m_centerDot.setOutlineColor(sf::Color::White);
    m_centerDot.setOutlineThickness(1.0f);

    m_pivotDot.setFillColor(sf::Color::Green);
    m_pivotDot.setOutlineColor(sf::Color::Black);
    m_pivotDot.setOutlineThickness(1.0f);

    m_baselineLine.setFillColor(sf::Color(255, 165, 0, 200));
}

void SpriteGizmoRenderer::SetHoveredSprite(const std::string& id) {
    m_hoveredId = id;
}

void SpriteGizmoRenderer::SetSelectedSprites(const std::vector<std::string>& ids) {
    m_selectedIds = ids;
}

void SpriteGizmoRenderer::Render(sf::RenderWindow& window, 
                                 const std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>& sprites, 
                                 float currentZoom,
                                 bool showBoxes,
                                 bool showCenters,
                                 bool showPivots,
                                 bool showBaselines,
                                 bool showIds,
                                 sf::Font* font) {
    
    float invZoom = std::max(1.0f, 1.0f / currentZoom);

    for (const auto& sprite : sprites) {
        const auto& rect = sprite->GetSourceRect();
        
        bool isHovered = (sprite->GetId() == m_hoveredId);
        bool isSelected = std::find(m_selectedIds.begin(), m_selectedIds.end(), sprite->GetId()) != m_selectedIds.end();
        bool isMultiSelect = m_selectedIds.size() > 1;

        if (showBoxes || isHovered || isSelected) {
            m_box.setPosition(rect.x, rect.y);
            m_box.setSize(sf::Vector2f(rect.width, rect.height));
            
            if (isSelected) {
                m_box.setOutlineColor(isMultiSelect ? sf::Color(255, 100, 100) : sf::Color::Yellow);
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

        if (showPivots || isSelected) {
            const auto& p = sprite->GetPivot();
            float px = rect.x + p.x * rect.width;
            float py = rect.y + p.y * rect.height;
            m_pivotDot.setRadius(3.0f * invZoom);
            m_pivotDot.setOrigin(m_pivotDot.getRadius(), m_pivotDot.getRadius());
            m_pivotDot.setPosition(px, py);
            window.draw(m_pivotDot);
        }

        if (showBaselines || isSelected) {
            float by = rect.y + rect.height - sprite->GetBaseline();
            m_baselineLine.setPosition(rect.x, by);
            m_baselineLine.setSize(sf::Vector2f(rect.width, 1.0f * invZoom));
            window.draw(m_baselineLine);
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