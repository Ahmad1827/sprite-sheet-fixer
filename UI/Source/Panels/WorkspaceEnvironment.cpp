#include "Panels/WorkspaceEnvironment.h"
#include "Theme.h"
#include <vector>

namespace StudioUI {

void WorkspaceEnvironment::InitializeFont(const std::string& fontPath) {
    std::vector<std::string> fontCandidates = { fontPath, "Resources/font.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" };
    for (const auto& path : fontCandidates) {
        if (!path.empty() && m_font.loadFromFile(path)) break;
    }
    
    m_statusBarBg.setFillColor(Theme::PanelBackground);
    m_statusBarTopBorder.setFillColor(Theme::BorderColor);

    m_statusText.setFont(m_font);
    m_statusText.setCharacterSize(12);
    m_statusText.setFillColor(Theme::TextPrimary);

    m_infoText.setFont(m_font);
    m_infoText.setCharacterSize(11);
    m_infoText.setFillColor(Theme::TextMuted);
}

void WorkspaceEnvironment::SetStatus(const std::string& hint, const std::string& info) {
    m_statusText.setString(hint);
    m_infoText.setString(info);
}

bool WorkspaceEnvironment::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    (void)window;
    if (m_showContextMenu) {
        // Use the position carried by the event itself, so a click always acts
        // on the item that was under it.
        sf::Vector2f mousePos(-1.0f, -1.0f);
        if (event.type == sf::Event::MouseMoved) {
            mousePos = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
        } else if (event.type == sf::Event::MouseButtonPressed) {
            mousePos = { static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y) };
        }

        float itemHeight = 28.0f;
        float menuWidth = 190.0f;
        float totalHeight = m_menuItems.size() * itemHeight;

        sf::FloatRect menuBounds(m_menuPosition.x, m_menuPosition.y, menuWidth, totalHeight);

        if (event.type == sf::Event::MouseMoved) {
            if (menuBounds.contains(mousePos)) {
                m_hoveredMenuItem = static_cast<int>((mousePos.y - m_menuPosition.y) / itemHeight);
            } else {
                m_hoveredMenuItem = -1;
            }
            return true;
        }

        if (event.type == sf::Event::MouseButtonPressed) {
            if (menuBounds.contains(mousePos)) {
                int index = static_cast<int>((mousePos.y - m_menuPosition.y) / itemHeight);
                if (index >= 0 && index < static_cast<int>(m_menuItems.size())) {
                    // The action may open another menu, so take it out first.
                    auto action = m_menuItems[index].action;
                    HideContextMenu();
                    if (action) action();
                    return true;
                }
            }
            HideContextMenu();
            return true;
        }
    }
    return false;
}

void WorkspaceEnvironment::ShowContextMenu(sf::Vector2f position, const std::vector<ContextMenuItem>& items) {
    m_menuPosition = position;
    m_menuItems = items;
    m_showContextMenu = true;
    m_hoveredMenuItem = -1;
}

void WorkspaceEnvironment::HideContextMenu() {
    m_showContextMenu = false;
    m_menuItems.clear();
}

void WorkspaceEnvironment::Render(sf::RenderWindow& window) {
    float yPos = m_bounds.top + m_bounds.height - Theme::StatusBarHeight;

    m_statusBarBg.setPosition(m_bounds.left, yPos);
    m_statusBarBg.setSize(sf::Vector2f(m_bounds.width, Theme::StatusBarHeight));

    m_statusBarTopBorder.setPosition(m_bounds.left, yPos);
    m_statusBarTopBorder.setSize(sf::Vector2f(m_bounds.width, Theme::BorderThickness));

    m_statusText.setPosition(m_bounds.left + 14.0f, yPos + 7.0f);
    sf::FloatRect infoBounds = m_infoText.getLocalBounds();
    m_infoText.setPosition(m_bounds.left + m_bounds.width - infoBounds.width - 14.0f, yPos + 8.0f);

    window.draw(m_statusBarBg);
    window.draw(m_statusBarTopBorder);
    window.draw(m_statusText);
    window.draw(m_infoText);

    if (m_showContextMenu) {
        float itemHeight = 28.0f;
        float menuWidth = 190.0f;
        float totalHeight = m_menuItems.size() * itemHeight;

        sf::RectangleShape menuBg(sf::Vector2f(menuWidth, totalHeight));
        menuBg.setPosition(m_menuPosition);
        menuBg.setFillColor(Theme::InspectorBackground);
        menuBg.setOutlineThickness(Theme::BorderThickness);
        menuBg.setOutlineColor(Theme::BorderColor);
        window.draw(menuBg);

        for (size_t i = 0; i < m_menuItems.size(); ++i) {
            float itemY = m_menuPosition.y + (i * itemHeight);

            if (static_cast<int>(i) == m_hoveredMenuItem) {
                sf::RectangleShape hoverRect(sf::Vector2f(menuWidth, itemHeight));
                hoverRect.setPosition(m_menuPosition.x, itemY);
                hoverRect.setFillColor(Theme::HoverColor);
                window.draw(hoverRect);
            }

            sf::Text itemText;
            itemText.setFont(m_font);
            itemText.setString(m_menuItems[i].label);
            itemText.setCharacterSize(12);
            itemText.setFillColor(Theme::TextPrimary);
            itemText.setPosition(m_menuPosition.x + 12.0f, itemY + 6.0f);
            window.draw(itemText);
        }
    }
}

}