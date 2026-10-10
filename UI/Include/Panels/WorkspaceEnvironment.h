#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <functional>

namespace StudioUI {

struct ContextMenuItem {
    std::string label;
    std::function<void()> action;
};

class WorkspaceEnvironment {
public:
    WorkspaceEnvironment() = default;

    void InitializeFont(const std::string& fontPath);
    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void Render(sf::RenderWindow& window);
    // hint: what to do next or how the active tool works. info: zoom, counts, cursor.
    void SetStatus(const std::string& hint, const std::string& info);
    void SetBounds(const sf::FloatRect& bounds) { m_bounds = bounds; }
    void ShowContextMenu(sf::Vector2f position, const std::vector<ContextMenuItem>& items);
    void HideContextMenu();

private:
    sf::FloatRect m_bounds{0.0f, 0.0f, 1280.0f, 720.0f};
    sf::Font m_font;
    sf::RectangleShape m_statusBarBg;
    sf::RectangleShape m_statusBarTopBorder;
    sf::Text m_statusText;
    sf::Text m_infoText;

    // Context Menu UI
    bool m_showContextMenu{false};
    sf::Vector2f m_menuPosition;
    std::vector<ContextMenuItem> m_menuItems;
    int m_hoveredMenuItem{-1};
};

} // namespace StudioUI