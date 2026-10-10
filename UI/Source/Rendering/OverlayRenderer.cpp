#include "Rendering/OverlayRenderer.h"
#include "Theme.h"
#include "Widgets.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace StudioUI {

OverlayRenderer::OverlayRenderer() = default;

bool OverlayRenderer::InitializeFont(const std::string& customPath) {
    m_hasFont = LoadUiFont(m_font, customPath);
    return m_hasFont;
}

void OverlayRenderer::DrawTextShadowed(sf::RenderWindow& window, const std::string& str, const sf::Vector2f& pos) {
    if (!m_hasFont) return;
    sf::Text text(str, m_font, 12);
    
    text.setPosition(pos.x + 1, pos.y + 1);
    text.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(text);

    text.setPosition(pos);
    text.setFillColor(Theme::TextPrimary);
    window.draw(text);
}

void OverlayRenderer::RenderDebug(sf::RenderWindow& window, const DebugInfo& info) {
    if (!m_hasFont) return;

    float panelWidth = 240.0f;
    float panelHeight = 160.0f;
    float posX = 12.0f;
    float posY = Theme::ToolbarHeight + 12.0f;

    sf::RectangleShape bg({panelWidth, panelHeight});
    bg.setPosition(posX, posY);
    bg.setFillColor(Theme::InspectorBackground);
    bg.setOutlineThickness(Theme::BorderThickness);
    bg.setOutlineColor(Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header({panelWidth, Theme::HeaderHeight});
    header.setPosition(posX, posY);
    header.setFillColor(Theme::PanelBackground);
    window.draw(header);

    sf::Text headerTitle;
    headerTitle.setFont(m_font);
    headerTitle.setString("DEBUG METRICS (F3)");
    headerTitle.setCharacterSize(11);
    headerTitle.setFillColor(Theme::TextSecondary);
    headerTitle.setPosition(posX + 8.0f, posY + 6.0f);
    window.draw(headerTitle);

    std::ostringstream oss;
    oss << "FPS: " << static_cast<int>(info.fps) << " (" << std::fixed << std::setprecision(1) << info.frameTimeMs << " ms)\n"
        << "Zoom: " << std::fixed << std::setprecision(2) << info.zoomLevel << "x\n"
        << "Pan: [" << static_cast<int>(info.panOffset.x) << ", " << static_cast<int>(info.panOffset.y) << "]\n"
        << "Canvas: " << info.imageWidth << "x" << info.imageHeight << "\n"
        << "Project: " << (info.projectLoaded ? "Active" : "None") << "\n"
        << "Texture: " << (info.textureLoaded ? "Loaded" : "Empty");

    sf::Text debugText;
    debugText.setFont(m_font);
    debugText.setString(oss.str());
    debugText.setCharacterSize(11);
    debugText.setFillColor(Theme::TextPrimary);
    debugText.setPosition(posX + 8.0f, posY + Theme::HeaderHeight + 6.0f);
    window.draw(debugText);
}

void OverlayRenderer::RenderInspector(sf::RenderWindow& window, const InspectorInfo& info) {
    if (!m_hasFont) return;

    float panelWidth = 220.0f;
    float panelHeight = 110.0f;
    float posX = 12.0f;
    float posY = window.getSize().y - Theme::StatusBarHeight - panelHeight - 12.0f;

    sf::RectangleShape bg({panelWidth, panelHeight});
    bg.setPosition(posX, posY);
    bg.setFillColor(Theme::InspectorBackground);
    bg.setOutlineThickness(Theme::BorderThickness);
    bg.setOutlineColor(Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header({panelWidth, Theme::HeaderHeight});
    header.setPosition(posX, posY);
    header.setFillColor(Theme::PanelBackground);
    window.draw(header);

    sf::Text headerTitle;
    headerTitle.setFont(m_font);
    headerTitle.setString("CANVAS INSPECTOR");
    headerTitle.setCharacterSize(11);
    headerTitle.setFillColor(Theme::TextSecondary);
    headerTitle.setPosition(posX + 8.0f, posY + 6.0f);
    window.draw(headerTitle);

    std::ostringstream oss;
    if (info.isHoveringImage) {
        oss << "Coords: [" << info.mouseImage.x << ", " << info.mouseImage.y << "]\n"
            << "RGBA: (" << (int)info.pixelColor.r << ", " << (int)info.pixelColor.g << ", " 
                        << (int)info.pixelColor.b << ", " << (int)info.pixelColor.a << ")\n"
            << "Zoom: " << std::fixed << std::setprecision(2) << info.currentZoom << "x";
    } else {
        oss << "Coords: [Out of bounds]\nRGBA: N/A\nZoom: " 
            << std::fixed << std::setprecision(2) << info.currentZoom << "x";
    }

    sf::Text contentText;
    contentText.setFont(m_font);
    contentText.setString(oss.str());
    contentText.setCharacterSize(11);
    contentText.setFillColor(Theme::TextPrimary);
    contentText.setPosition(posX + 8.0f, posY + Theme::HeaderHeight + 6.0f);
    window.draw(contentText);
}

void OverlayRenderer::RenderProgress(sf::RenderWindow& window, const JobProgressInfo& info) {
    if (!info.isRunning || !m_hasFont) return;
    
    sf::Vector2f center(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
    
    float boxWidth = 340.0f;
    float boxHeight = 80.0f;

    sf::RectangleShape panel({boxWidth, boxHeight});
    panel.setOrigin(boxWidth / 2.0f, boxHeight / 2.0f);
    panel.setPosition(center);
    panel.setFillColor(Theme::InspectorBackground);
    panel.setOutlineThickness(Theme::BorderThickness);
    panel.setOutlineColor(Theme::BorderColor);
    window.draw(panel);

    std::ostringstream oss;
    oss << "Detecting Sprites... " << static_cast<int>(info.progress * 100) << "%";
    
    sf::Text titleText(oss.str(), m_font, 12);
    titleText.setFillColor(Theme::TextPrimary);
    sf::FloatRect tBounds = titleText.getLocalBounds();
    titleText.setPosition(center.x - (tBounds.width / 2.0f), center.y - 24.0f);
    window.draw(titleText);

    // Progress bar container
    float barWidth = 300.0f;
    float barHeight = 8.0f;

    sf::RectangleShape bgBar({barWidth, barHeight});
    bgBar.setOrigin(barWidth / 2.0f, barHeight / 2.0f);
    bgBar.setPosition(center.x, center.y + 10.0f);
    bgBar.setFillColor(Theme::PanelBackground);
    window.draw(bgBar);
    
    sf::RectangleShape fgBar({barWidth * info.progress, barHeight});
    fgBar.setPosition(center.x - (barWidth / 2.0f), center.y + 10.0f - (barHeight / 2.0f));
    fgBar.setFillColor(Theme::AccentColor);
    window.draw(fgBar);
}

void OverlayRenderer::RenderSpriteInspector(sf::RenderWindow& window, const SpriteInspectorInfo& info) {
    if (!info.isActive || !m_hasFont) return;
    
    float panelWidth = 230.0f;
    float panelHeight = 120.0f;
    float posX = window.getSize().x - panelWidth - 12.0f;
    float posY = Theme::ToolbarHeight + 12.0f;

    sf::RectangleShape bg({panelWidth, panelHeight});
    bg.setPosition(posX, posY);
    bg.setFillColor(Theme::InspectorBackground);
    bg.setOutlineThickness(Theme::BorderThickness);
    bg.setOutlineColor(Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header({panelWidth, Theme::HeaderHeight});
    header.setPosition(posX, posY);
    header.setFillColor(Theme::PanelBackground);
    window.draw(header);

    sf::Text headerTitle;
    headerTitle.setFont(m_font);
    headerTitle.setString("SELECTED SPRITE");
    headerTitle.setCharacterSize(11);
    headerTitle.setFillColor(Theme::TextSecondary);
    headerTitle.setPosition(posX + 8.0f, posY + 6.0f);
    window.draw(headerTitle);

    std::ostringstream oss;
    oss << "ID: " << info.id << "\n"
        << "Rect: " << static_cast<int>(info.x) << ", " << static_cast<int>(info.y) 
        << " [" << static_cast<int>(info.w) << "x" << static_cast<int>(info.h) << "]\n"
        << "Pixels: " << info.pixelCount << "\n"
        << "Center: " << std::fixed << std::setprecision(1) << info.cx << ", " << info.cy;

    sf::Text contentText;
    contentText.setFont(m_font);
    contentText.setString(oss.str());
    contentText.setCharacterSize(11);
    contentText.setFillColor(Theme::TextPrimary);
    contentText.setPosition(posX + 8.0f, posY + Theme::HeaderHeight + 6.0f);
    window.draw(contentText);
}

void OverlayRenderer::RenderEmptyState(sf::RenderWindow& window, const sf::FloatRect& area) {
    if (!m_hasFont) return;

    const sf::Vector2f center(area.left + area.width / 2.0f, area.top + area.height / 2.0f);
    auto centered = [&](sf::Text& text, float y) {
        sf::FloatRect b = text.getLocalBounds();
        text.setPosition(std::floor(center.x - b.width / 2.0f - b.left), std::floor(y));
        window.draw(text);
    };

    // Drop zone
    const sf::FloatRect zone(std::floor(center.x - 250.0f), std::floor(center.y - 150.0f), 500.0f, 190.0f);
    DrawRoundedRect(window, zone, 14.0f, Theme::InspectorBackground, Theme::BorderColor, 1.5f);

    sf::Text title("Drop a sprite sheet here", m_font, 24);
    title.setFillColor(Theme::TextPrimary);
    centered(title, zone.top + 48.0f);

    sf::Text subtitle("or press Open  (Ctrl+O)", m_font, 14);
    subtitle.setFillColor(Theme::TextSecondary);
    centered(subtitle, zone.top + 92.0f);

    sf::Text formats("PNG  JPG  BMP  WebP", m_font, 11);
    formats.setFillColor(Theme::TextMuted);
    formats.setLetterSpacing(1.8f);
    centered(formats, zone.top + 138.0f);

    // The whole workflow in four steps
    const char* steps[4] = { "Clean", "Detect", "Animate", "Export" };
    const char* notes[4] = { "remove the background", "find every sprite", "group frames", "save the sheet" };
    const float stepWidth = 150.0f;
    const float startX = center.x - stepWidth * 2.0f;
    const float y = zone.top + zone.height + 36.0f;
    for (int i = 0; i < 4; ++i) {
        const float cx = startX + stepWidth * (i + 0.5f);

        sf::CircleShape badge(13.0f, 40);
        badge.setOrigin(13.0f, 13.0f);
        badge.setPosition(std::floor(cx), y);
        badge.setFillColor(sf::Color::Transparent);
        badge.setOutlineThickness(1.5f);
        badge.setOutlineColor(Theme::AccentColor);
        window.draw(badge);

        sf::Text number(std::to_string(i + 1), m_font, 13);
        number.setFillColor(Theme::TextAccent);
        sf::FloatRect nb = number.getLocalBounds();
        number.setPosition(std::floor(cx - nb.width / 2.0f - nb.left), std::floor(y - nb.height / 2.0f - nb.top));
        window.draw(number);

        sf::Text name(steps[i], m_font, 14);
        name.setFillColor(Theme::TextPrimary);
        sf::FloatRect sb = name.getLocalBounds();
        name.setPosition(std::floor(cx - sb.width / 2.0f - sb.left), std::floor(y + 24.0f));
        window.draw(name);

        sf::Text note(notes[i], m_font, 11);
        note.setFillColor(Theme::TextMuted);
        sf::FloatRect ob = note.getLocalBounds();
        note.setPosition(std::floor(cx - ob.width / 2.0f - ob.left), std::floor(y + 46.0f));
        window.draw(note);

        if (i < 3) {
            sf::RectangleShape link({ stepWidth - 44.0f, 1.0f });
            link.setPosition(std::floor(cx + 22.0f), y);
            link.setFillColor(Theme::BorderColor);
            window.draw(link);
        }
    }
}

} // namespace StudioUI
