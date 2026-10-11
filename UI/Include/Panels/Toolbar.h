#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

namespace StudioUI {

struct ToolbarAction {
    std::string id;
    std::string label;
    std::string group;      // caption shown above the buttons that share it
    std::string tooltip;    // one line explaining what the button does
    std::function<void()> onClick;
    std::function<bool()> isActive;   // highlighted while this returns true (tools, toggles)
    std::function<bool()> isEnabled;  // greyed out and ignored while this returns false
    std::function<bool()> isPrimary;  // the step to take next, drawn in the accent colour
    bool alignRight{false};
};

class Toolbar {
public:
    Toolbar() = default;
    void SetBounds(const sf::FloatRect& bounds) { m_bounds = bounds; }
    void Initialize(const std::string& fontPath, std::vector<ToolbarAction> actions);

    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void Render(sf::RenderWindow& window);
    // Forget where the mouse was, so no button stays hovered or shows its tooltip.
    void ClearHover() { m_mousePos = { -1.0f, -1.0f }; }

private:
    struct Button {
        ToolbarAction action;
        sf::FloatRect bounds;
        float hover{0.0f};
    };
    struct Caption {
        std::string text;
        float x;
    };

    void Layout();
    int ButtonAt(sf::Vector2f pos) const;
    static bool Enabled(const Button& button) { return !button.action.isEnabled || button.action.isEnabled(); }

    sf::Font m_font;
    std::vector<Button> m_buttons;
    std::vector<Caption> m_captions;
    sf::Vector2f m_mousePos{-1.0f, -1.0f};
    int m_hoveredIndex{-1};
    float m_hoverTime{0.0f};
    sf::Clock m_clock;
    sf::FloatRect m_bounds{0.0f, 0.0f, 1280.0f, 720.0f};
};

}
