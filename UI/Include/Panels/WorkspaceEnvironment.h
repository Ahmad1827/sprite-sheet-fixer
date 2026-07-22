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
    void InitializeFont(const std::string& fontPath);
    void UpdateStatusBar(float zoom, const sf::Vector2f& mousePos, int selectedCount, const std::string& currentAnim, int currentFrame, const std::string& exportRes);
    void ShowContextMenu(const sf::Vector2f& position, const std::vector<ContextMenuItem>& items);
    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void Render(sf::RenderWindow& window);

private:
    sf::Font m_font;
    sf::RectangleShape m_statusBarBg;
    sf::Text m_statusText;

    bool m_contextMenuVisible{false};
    sf::RectangleShape m_contextMenuBg;
    std::vector<ContextMenuItem> m_contextMenuItems;
    std::vector<sf::Text> m_contextMenuTexts;
};

}