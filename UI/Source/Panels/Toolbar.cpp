#include "Panels/Toolbar.h"
#include "Theme.h"
#include "StudioEngineFacade.h"
#include <iostream>
#include <cmath>

namespace StudioUI {

void Toolbar::Initialize(const std::string& fontPath,
                         std::function<void()> onOpenImage,
                         std::function<void()> onLoadProject,
                         std::function<void()> onToggleUI,
                         std::function<void()> onOpenWizard) {
    
    // Robust font loading with fallback candidates
    std::vector<std::string> fontCandidates = {
        fontPath,
        "Resources/font.ttf",
        "../Resources/font.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
    };

    bool fontLoaded = false;
    for (const auto& path : fontCandidates) {
        if (m_font.loadFromFile(path)) {
            fontLoaded = true;
            break;
        }
    }

    if (!fontLoaded) {
        std::cerr << "[Toolbar] Warning: Failed to load any font candidate." << std::endl;
    }

    m_buttons.clear();

    // Group 1: File Actions
    m_buttons.push_back({"open_img", "Import Image", {}, onOpenImage});
    m_buttons.push_back({"load_proj", "Open Project", {}, onLoadProject});

    // Group 2: Actions & Auto Detection
    m_buttons.push_back({"wizard", "Anim Wizard", {}, onOpenWizard});

    // Group 3: View Options
    m_buttons.push_back({"toggle_ui", "Hide UI", {}, onToggleUI, true, false});

    LayoutButtons(1280.0f);
}

void Toolbar::LayoutButtons(float windowWidth) {
    m_background.setSize({windowWidth, Theme::ToolbarHeight});
    m_background.setFillColor(Theme::PanelBackground);
    m_background.setPosition(0.0f, 0.0f);

    m_bottomBorder.setSize({windowWidth, Theme::BorderThickness});
    m_bottomBorder.setFillColor(Theme::BorderColor);
    m_bottomBorder.setPosition(0.0f, Theme::ToolbarHeight - Theme::BorderThickness);

    float startX = 10.0f;
    float buttonHeight = Theme::ToolbarHeight - 8.0f;
    float buttonPaddingX = 14.0f;
    float spacing = 6.0f;

    m_dividers.clear();

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        sf::Text tempText;
        tempText.setFont(m_font);
        tempText.setString(m_buttons[i].label);
        tempText.setCharacterSize(13);

        float textWidth = tempText.getLocalBounds().width;
        float buttonWidth = textWidth + (buttonPaddingX * 2.0f);

        m_buttons[i].bounds = sf::FloatRect(startX, 4.0f, buttonWidth, buttonHeight);
        startX += buttonWidth + spacing;

        if (m_buttons[i].id == "load_proj" || m_buttons[i].id == "wizard") {
            sf::RectangleShape divider({Theme::BorderThickness, buttonHeight - 4.0f});
            divider.setPosition(startX - (spacing / 2.0f), 6.0f);
            divider.setFillColor(Theme::BorderColor);
            m_dividers.push_back(divider);
        }
    }
}

void Toolbar::Update(float deltaTime, sf::Vector2f mousePos) {
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        bool isHovered = m_buttons[i].bounds.contains(mousePos);
        float targetAlpha = isHovered ? 1.0f : 0.0f;

        float speed = 10.0f;
        if (m_buttons[i].hoverAlpha < targetAlpha) {
            m_buttons[i].hoverAlpha = std::min(targetAlpha, m_buttons[i].hoverAlpha + deltaTime * speed);
        } else if (m_buttons[i].hoverAlpha > targetAlpha) {
            m_buttons[i].hoverAlpha = std::max(targetAlpha, m_buttons[i].hoverAlpha - deltaTime * speed);
        }
    }
}

bool Toolbar::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine) {
    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    sf::Vector2f mousePos(mousePixel.x, mousePixel.y);

    if (mousePos.y > Theme::ToolbarHeight) {
        m_hoveredIndex = -1;
        return false;
    }

    if (event.type == sf::Event::MouseMoved) {
        m_hoveredIndex = -1;
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i].bounds.contains(mousePos)) {
                m_hoveredIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i].bounds.contains(mousePos)) {
                if (m_buttons[i].isToggle) {
                    m_buttons[i].isToggled = !m_buttons[i].isToggled;
                }
                if (m_buttons[i].onClick) {
                    m_buttons[i].onClick();
                }
                return true;
            }
        }
    }

    return false;
}

void Toolbar::Render(sf::RenderWindow& window) {
    sf::Vector2u winSize = window.getSize();
    LayoutButtons(static_cast<float>(winSize.x));

    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    sf::Vector2f mousePos(mousePixel.x, mousePixel.y);
    Update(0.016f, mousePos);

    window.draw(m_background);
    window.draw(m_bottomBorder);

    for (const auto& divider : m_dividers) {
        window.draw(divider);
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        const auto& btn = m_buttons[i];

        sf::RectangleShape bg({btn.bounds.width, btn.bounds.height});
        bg.setPosition(btn.bounds.left, btn.bounds.top);

        if (btn.isToggled) {
            bg.setFillColor(Theme::AccentColor);
        } else {
            sf::Color base = Theme::PanelBackground;
            sf::Color hover = Theme::HoverColor;
            float t = btn.hoverAlpha;

            sf::Color blended(
                static_cast<sf::Uint8>(base.r + (hover.r - base.r) * t),
                static_cast<sf::Uint8>(base.g + (hover.g - base.g) * t),
                static_cast<sf::Uint8>(base.b + (hover.b - base.b) * t)
            );
            bg.setFillColor(blended);
        }

        bg.setOutlineThickness(Theme::BorderThickness);
        bg.setOutlineColor(btn.isToggled ? Theme::AccentHoverColor : Theme::BorderColor);
        window.draw(bg);

        sf::Text labelText;
        labelText.setFont(m_font);
        labelText.setString(btn.label);
        labelText.setCharacterSize(13);
        labelText.setFillColor(btn.isToggled ? sf::Color::White : Theme::TextPrimary);

        sf::FloatRect textBounds = labelText.getLocalBounds();
        float textX = std::floor(btn.bounds.left + (btn.bounds.width - textBounds.width) / 2.0f - textBounds.left);
        float textY = std::floor(btn.bounds.top + (btn.bounds.height - textBounds.height) / 2.0f - textBounds.top - 1.0f);
        labelText.setPosition(textX, textY);

        window.draw(labelText);
    }
}

} // namespace StudioUI