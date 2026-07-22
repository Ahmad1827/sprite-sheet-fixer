#include "Rendering/SpriteGizmoRenderer.h"
#include "DataModels/SpriteDefinition.h"
#include "Theme.h"

namespace StudioUI {

SpriteGizmoRenderer::SpriteGizmoRenderer() = default;

void SpriteGizmoRenderer::SetHoveredSprite(const std::string& id) {
    m_hoveredId = id;
}

void SpriteGizmoRenderer::SetSelectedSprites(const std::vector<std::string>& ids) {
    m_selectedIds = ids;
}

void SpriteGizmoRenderer::Render(
    sf::RenderWindow& window,
    const std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>& sprites,
    float currentZoom,
    bool showBoxes,
    bool showCenters,
    bool showPivots,
    bool showBaselines,
    bool showIds,
    sf::Font* font
) {
    for (const auto& sprite : sprites) {
        if (!sprite) continue;

        std::string id = sprite->GetId();
        auto rect = sprite->GetSourceRect();

        bool isSelected = false;
        for (const auto& selId : m_selectedIds) {
            if (selId == id) {
                isSelected = true;
                break;
            }
        }
        bool isHovered = (id == m_hoveredId);

        // 1. BOUNDING BOXES
        if (showBoxes || isSelected || isHovered) {
            sf::RectangleShape box(sf::Vector2f(rect.width, rect.height));
            box.setPosition(rect.x, rect.y);
            box.setFillColor(sf::Color::Transparent);

            if (isSelected) {
                box.setOutlineThickness(2.0f / currentZoom);
                box.setOutlineColor(Theme::AccentColor);
            } else if (isHovered) {
                box.setOutlineThickness(1.5f / currentZoom);
                box.setOutlineColor(Theme::HoverColor);
            } else {
                box.setOutlineThickness(1.0f / currentZoom);
                box.setOutlineColor(sf::Color(Theme::BorderColor.r, Theme::BorderColor.g, Theme::BorderColor.b, 120));
            }
            window.draw(box);
        }

        // 2. PIVOT POINT
        if (showPivots || isSelected) {
            auto pivot = sprite->GetPivot();
            float pivotX = rect.x + pivot.x;
            float pivotY = rect.y + pivot.y;

            float crossSize = 5.0f / currentZoom;
            
            sf::RectangleShape lineH(sf::Vector2f(crossSize * 2.0f, 1.0f / currentZoom));
            lineH.setOrigin(crossSize, 0.5f / currentZoom);
            lineH.setPosition(pivotX, pivotY);
            lineH.setFillColor(isSelected ? Theme::AccentColor : Theme::TextSecondary);

            sf::RectangleShape lineV(sf::Vector2f(1.0f / currentZoom, crossSize * 2.0f));
            lineV.setOrigin(0.5f / currentZoom, crossSize);
            lineV.setPosition(pivotX, pivotY);
            lineV.setFillColor(isSelected ? Theme::AccentColor : Theme::TextSecondary);

            window.draw(lineH);
            window.draw(lineV);
        }

        // 3. BASELINE
        if (showBaselines) {
            float baselineY = rect.y + rect.height - sprite->GetBaseline();

            sf::RectangleShape line(sf::Vector2f(rect.width, 1.0f / currentZoom));
            line.setPosition(rect.x, baselineY);
            line.setFillColor(sf::Color(235, 87, 87, 180));
            window.draw(line);
        }
    }
}

} // namespace StudioUI