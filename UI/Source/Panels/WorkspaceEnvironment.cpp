#include "Panels/WorkspaceEnvironment.h"

namespace StudioUI {

void WorkspaceEnvironment::InitializeFont(const std::string& fontPath) {
    if (!m_font.loadFromFile(fontPath)) {
        m_font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    }
    m_statusBarBg.setFillColor(sf::Color(35, 35, 40));
    m_statusText.setFont(m_font);
    m_statusText.setCharacterSize(12);
    m_statusText.setFillColor(sf::Color::White);
    
    m_contextMenuBg.setFillColor(sf::Color(45, 45, 50));
    m_contextMenuBg.setOutlineColor(sf::Color(80, 80, 90));
    m_contextMenuBg.setOutlineThickness(1.f);
}

void WorkspaceEnvironment::UpdateStatusBar(float zoom, const sf::Vector2f& mousePos, int selectedCount, const std::string& currentAnim, int currentFrame, const std::string& exportRes) {
    std::string text = "Zoom: " + std::to_string(zoom).substr(0, 4) + "x | Pos: [" + 
                       std::to_string(static_cast<int>(mousePos.x)) + ", " + 
                       std::to_string(static_cast<int>(mousePos.y)) + "] | Sel: " + 
                       std::to_string(selectedCount) + " | Anim: " + currentAnim + 
                       " | Frm: " + std::to_string(currentFrame) + " | Exp: " + exportRes;
    m_statusText.setString(text);
}

void WorkspaceEnvironment::ShowContextMenu(const sf::Vector2f& position, const std::vector<ContextMenuItem>& items) {
    m_contextMenuItems = items;
    m_contextMenuTexts.clear();
    float currentY = position.y + 5.f;
    float maxWidth = 120.f;

    for (const auto& item : items) {
        sf::Text t(item.label, m_font, 12);
        t.setPosition(position.x + 10.f, currentY);
        t.setFillColor(sf::Color::White);
        m_contextMenuTexts.push_back(t);
        if (t.getLocalBounds().width + 20.f > maxWidth) {
            maxWidth = t.getLocalBounds().width + 20.f;
        }
        currentY += 25.f;
    }

    m_contextMenuBg.setPosition(position);
    m_contextMenuBg.setSize({maxWidth, currentY - position.y + 5.f});
    m_contextMenuVisible = true;
}

bool WorkspaceEnvironment::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_contextMenuVisible) return false;

    if (event.type == sf::Event::MouseButtonPressed) {
        sf::Vector2f pos(event.mouseButton.x, event.mouseButton.y);
        if (m_contextMenuBg.getGlobalBounds().contains(pos)) {
            for (size_t i = 0; i < m_contextMenuTexts.size(); ++i) {
                sf::FloatRect bounds;
                bounds.left = m_contextMenuBg.getPosition().x;
                bounds.top = m_contextMenuBg.getPosition().y + 5.f + (i * 25.f);
                bounds.width = m_contextMenuBg.getSize().x;
                bounds.height = 25.f;

                if (bounds.contains(pos)) {
                    m_contextMenuItems[i].action();
                    m_contextMenuVisible = false;
                    return true;
                }
            }
        }
        m_contextMenuVisible = false;
    }
    return false;
}

void WorkspaceEnvironment::Render(sf::RenderWindow& window) {
    sf::Vector2u size = window.getSize();
    m_statusBarBg.setSize({static_cast<float>(size.x), 25.f});
    m_statusBarBg.setPosition(0.f, static_cast<float>(size.y) - 25.f);
    m_statusText.setPosition(10.f, static_cast<float>(size.y) - 20.f);

    window.draw(m_statusBarBg);
    window.draw(m_statusText);

    if (m_contextMenuVisible) {
        window.draw(m_contextMenuBg);
        for (const auto& t : m_contextMenuTexts) {
            window.draw(t);
        }
    }
}

}