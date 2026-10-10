#include "Panels/ExportPreviewPanel.h"
#include "StudioEngineFacade.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include "Utils/NativeFileDialog.h"
#include "Theme.h"
#include "Widgets.h"
#include <algorithm>
#include <cmath>

namespace StudioUI {

ExportPreviewPanel::ExportPreviewPanel() {
    m_view.setSize(1280.f, 720.f);
    m_view.setCenter(640.f, 360.f);
}

bool ExportPreviewPanel::InitializeFont(const std::string& customPath) {
    m_hasFont = LoadUiFont(m_font, customPath);
    return m_hasFont;
}

namespace {
constexpr float kBarHeight = 64.0f;
constexpr float kTitleHeight = 56.0f;
}

sf::FloatRect ExportPreviewPanel::PackedRect() const {
    return sf::FloatRect(20.0f, m_windowSize.y - kBarHeight + 15.0f, 150.0f, 34.0f);
}

sf::FloatRect ExportPreviewPanel::OriginalRect() const {
    return sf::FloatRect(174.0f, m_windowSize.y - kBarHeight + 15.0f, 150.0f, 34.0f);
}

sf::FloatRect ExportPreviewPanel::SaveRect() const {
    return sf::FloatRect(m_windowSize.x - 170.0f, m_windowSize.y - kBarHeight + 15.0f, 150.0f, 34.0f);
}

sf::FloatRect ExportPreviewPanel::CancelRect() const {
    return sf::FloatRect(m_windowSize.x - 270.0f, m_windowSize.y - kBarHeight + 15.0f, 90.0f, 34.0f);
}

void ExportPreviewPanel::RefreshPreview(const StudioCore::StudioEngineFacade& engine) {
    sf::Image img = engine.GenerateExportPreview(8, m_keepOriginalResolution);
    if (img.getSize().x > 0) {
        m_exportTexture.loadFromImage(img);
        m_exportSprite.setTexture(m_exportTexture, true);
        
        sf::FloatRect bounds = m_exportSprite.getLocalBounds();
        m_exportSprite.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
        m_exportSprite.setPosition(0.0f, 0.0f);
    }
    m_needsFit = true;
}

void ExportPreviewPanel::Activate(const StudioCore::StudioEngineFacade& engine) {
    m_isActive = true;
    m_keepOriginalResolution = false;
    m_lastExportPath.clear();
    RefreshPreview(engine);
    m_view.setCenter(0.0f, 0.0f);
    m_currentZoom = 1.0f;
}

void ExportPreviewPanel::Save(const StudioCore::StudioEngineFacade& engine) {
    std::string defaultName = m_keepOriginalResolution ? "cleaned_original_canvas.png" : "aligned_spritesheet.png";
    std::string savePath = NativeFileDialog::SaveFileDialog(defaultName.c_str());

    if (!savePath.empty()) {
        if (engine.ExportPNG(savePath, 8, m_keepOriginalResolution)) {
            std::cout << "[OK] Exported successfully to: " << savePath << std::endl;
            m_lastExportPath = savePath;
            m_isActive = false;
        } else {
            std::cerr << "[X] Failed to save exported image." << std::endl;
        }
    }
}

bool ExportPreviewPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    if (!m_isActive) return false;
    m_windowSize = { static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y) };

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            m_isActive = false;
        } else if (event.key.code == sf::Keyboard::Tab) {
            m_keepOriginalResolution = !m_keepOriginalResolution;
            RefreshPreview(engine);
        } else if (event.key.code == sf::Keyboard::Enter) {
            Save(engine);
        }
    } else if (event.type == sf::Event::MouseWheelScrolled) {
        float zoomFactor = (event.mouseWheelScroll.delta > 0) ? 0.8f : 1.25f;
        m_currentZoom *= zoomFactor;
    } else if (event.type == sf::Event::MouseButtonPressed) {
        sf::Vector2f pos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
        if (event.mouseButton.button == sf::Mouse::Left && pos.y >= m_windowSize.y - kBarHeight) {
            if (PackedRect().contains(pos) && m_keepOriginalResolution) {
                m_keepOriginalResolution = false;
                RefreshPreview(engine);
            } else if (OriginalRect().contains(pos) && !m_keepOriginalResolution) {
                m_keepOriginalResolution = true;
                RefreshPreview(engine);
            } else if (SaveRect().contains(pos)) {
                Save(engine);
            } else if (CancelRect().contains(pos)) {
                m_isActive = false;
            }
            return true;
        }
        if (event.mouseButton.button == sf::Mouse::Middle || event.mouseButton.button == sf::Mouse::Left) {
            m_isPanning = true;
            m_lastMousePos = sf::Mouse::getPosition(window);
        }
    } else if (event.type == sf::Event::MouseButtonReleased) {
        if (event.mouseButton.button == sf::Mouse::Middle || event.mouseButton.button == sf::Mouse::Left) {
            m_isPanning = false;
        }
    } else if (event.type == sf::Event::MouseMoved) {
        m_mousePos = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
        if (m_isPanning) {
            sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
            sf::Vector2f delta(
                static_cast<float>(m_lastMousePos.x - currentMousePos.x) * m_currentZoom,
                static_cast<float>(m_lastMousePos.y - currentMousePos.y) * m_currentZoom
            );
            m_view.move(delta);
            m_lastMousePos = currentMousePos;
        }
    }
    
    return true; 
}

void ExportPreviewPanel::Render(sf::RenderWindow& window) {
    if (!m_isActive) return;
    m_windowSize = { static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y) };

    sf::FloatRect bounds = m_exportSprite.getLocalBounds();
    if (m_needsFit && bounds.width > 0.0f && bounds.height > 0.0f) {
        const float availableW = m_windowSize.x * 0.9f;
        const float availableH = (m_windowSize.y - kBarHeight - kTitleHeight) * 0.9f;
        m_currentZoom = std::max(bounds.width / availableW, bounds.height / availableH);
        // Nudge the image up so it is centred in the space above the bar.
        m_view.setCenter(0.0f, (kBarHeight - kTitleHeight) / 2.0f * m_currentZoom);
        m_needsFit = false;
    }
    sf::Vector2f center = m_view.getCenter();
    m_view.setSize(m_windowSize.x * m_currentZoom, m_windowSize.y * m_currentZoom);
    m_view.setCenter(center);
    window.setView(m_view);

    sf::RectangleShape imageBackground(sf::Vector2f(bounds.width, bounds.height));
    imageBackground.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
    imageBackground.setPosition(0.0f, 0.0f);
    imageBackground.setFillColor(sf::Color(27, 29, 35));
    imageBackground.setOutlineThickness(m_currentZoom);
    imageBackground.setOutlineColor(Theme::BorderColor);
    window.draw(imageBackground);

    window.draw(m_exportSprite);

    window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, m_windowSize.x, m_windowSize.y)));
    if (!m_hasFont) return;

    sf::RectangleShape titleBar({ m_windowSize.x, kTitleHeight });
    titleBar.setFillColor(Theme::PanelBackground);
    window.draw(titleBar);

    sf::Text title("Export preview", m_font, 18);
    title.setFillColor(Theme::TextPrimary);
    title.setPosition(20.0f, 9.0f);
    window.draw(title);

    sf::Text note("This is exactly what will be saved:  "
                  + std::to_string(static_cast<int>(bounds.width)) + " x " + std::to_string(static_cast<int>(bounds.height))
                  + " px.     Scroll to zoom, drag to move.", m_font, 12);
    note.setFillColor(Theme::TextSecondary);
    note.setPosition(20.0f, 33.0f);
    window.draw(note);

    sf::RectangleShape bar({ m_windowSize.x, kBarHeight });
    bar.setPosition(0.0f, m_windowSize.y - kBarHeight);
    bar.setFillColor(Theme::PanelBackground);
    window.draw(bar);
    sf::RectangleShape barBorder({ m_windowSize.x, 1.0f });
    barBorder.setPosition(0.0f, m_windowSize.y - kBarHeight);
    barBorder.setFillColor(Theme::BorderColor);
    window.draw(barBorder);

    auto button = [&](const sf::FloatRect& rect, const std::string& label, bool primary, bool selected) {
        const bool hovered = rect.contains(m_mousePos);
        sf::Color fill = hovered ? Theme::BevelLight : Theme::HoverColor;
        sf::Color outline = Theme::BorderColor;
        sf::Color textColor = Theme::TextPrimary;
        if (primary) {
            fill = hovered ? Theme::AccentHoverColor : Theme::AccentColor;
            outline = fill;
            textColor = sf::Color(10, 18, 30);
        } else if (selected) {
            fill = MixColor(Theme::PanelBackground, Theme::AccentColor, 0.32f);
            outline = Theme::AccentColor;
            textColor = sf::Color::White;
        }
        DrawRoundedRect(window, rect, Theme::CornerRadius, fill, outline, 1.0f);
        sf::Text text(label, m_font, 13);
        text.setFillColor(textColor);
        sf::FloatRect b = text.getLocalBounds();
        text.setPosition(std::floor(rect.left + (rect.width - b.width) / 2.0f - b.left),
                         std::floor(rect.top + (rect.height - b.height) / 2.0f - b.top));
        window.draw(text);
    };

    button(PackedRect(), "Packed sheet", false, !m_keepOriginalResolution);
    button(OriginalRect(), "Original layout", false, m_keepOriginalResolution);
    button(CancelRect(), "Cancel", false, false);
    button(SaveRect(), "Save PNG", true, false);

    sf::Text hint(m_keepOriginalResolution ? "Keeps every sprite where it is on the sheet."
                                           : "Sprites are aligned and packed tightly together.", m_font, 12);
    hint.setFillColor(Theme::TextMuted);
    hint.setPosition(344.0f, m_windowSize.y - kBarHeight + 24.0f);
    window.draw(hint);
}

}
