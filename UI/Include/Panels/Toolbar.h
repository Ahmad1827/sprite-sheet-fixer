#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

namespace StudioCore {
    class StudioEngineFacade;
}

namespace StudioUI {

struct ToolbarButton {
    std::string id;
    std::string label;
    sf::FloatRect bounds;
    std::function<void()> onClick;
    bool isToggle{false};
    bool isToggled{false};
    float hoverAlpha{0.0f}; // Smooth hover transition state
};

class Toolbar {
public:
    Toolbar() = default;

    void Initialize(const std::string& fontPath,
                std::function<void()> onOpenImage,
                std::function<void()> onLoadProject,
                std::function<void()> onSaveProject,
                std::function<void()> onExport,
                std::function<void()> onToggleUI,
                std::function<void()> onOpenWizard);

    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine);
    void Update(float deltaTime, sf::Vector2f mousePos);
    void Render(sf::RenderWindow& window);

private:
    sf::Font m_font;
    sf::RectangleShape m_background;
    sf::RectangleShape m_bottomBorder;
    std::vector<ToolbarButton> m_buttons;
    std::vector<sf::RectangleShape> m_dividers;
    int m_hoveredIndex{-1};

    void LayoutButtons(float windowWidth);
};

} // namespace StudioUI