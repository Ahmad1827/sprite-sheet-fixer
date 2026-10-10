#include "Panels/Toolbar.h"
#include "Theme.h"
#include "Widgets.h"
#include <cmath>

namespace StudioUI {

namespace {
constexpr float kCaptionY = 7.0f;
constexpr float kButtonY = 24.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kButtonGap = 4.0f;
constexpr float kGroupGap = 18.0f;
constexpr float kMargin = 12.0f;
constexpr unsigned kLabelSize = 13;
constexpr unsigned kCaptionSize = 10;
}

void Toolbar::Initialize(const std::string& fontPath, std::vector<ToolbarAction> actions) {
    LoadUiFont(m_font, fontPath);

    m_buttons.clear();
    for (auto& action : actions) {
        Button button;
        button.action = std::move(action);
        m_buttons.push_back(std::move(button));
    }
    Layout();
}

void Toolbar::Layout() {
    m_captions.clear();

    auto naturalWidth = [&](const Button& b, float padding) {
        sf::Text text(b.action.label, m_font, kLabelSize);
        return std::max(46.0f, std::ceil(text.getLocalBounds().width) + padding * 2.0f);
    };

    // Shrink the padding when the window is too narrow to fit every button.
    float padding = 13.0f;
    for (; padding > 4.0f; padding -= 1.0f) {
        float total = kMargin * 2.0f;
        std::string lastGroup;
        for (const auto& b : m_buttons) {
            total += naturalWidth(b, padding) + kButtonGap;
            if (b.action.group != lastGroup) total += kGroupGap;
            lastGroup = b.action.group;
        }
        if (total <= m_bounds.width) break;
    }

    float x = m_bounds.left + kMargin;
    std::string group;
    for (auto& b : m_buttons) {
        if (b.action.alignRight) continue;
        if (b.action.group != group) {
            if (!group.empty()) x += kGroupGap - kButtonGap;
            group = b.action.group;
            m_captions.push_back({ group, x });
        }
        float width = naturalWidth(b, padding);
        b.bounds = sf::FloatRect(std::floor(x), kButtonY, width, kButtonHeight);
        x += width + kButtonGap;
    }

    float right = m_bounds.left + m_bounds.width - kMargin;
    group.clear();
    for (auto it = m_buttons.rbegin(); it != m_buttons.rend(); ++it) {
        if (!it->action.alignRight) continue;
        if (it->action.group != group) {
            if (!group.empty()) {
                m_captions.push_back({ group, right + kButtonGap });
                right -= kGroupGap - kButtonGap;
            }
            group = it->action.group;
        }
        float width = naturalWidth(*it, padding);
        right -= width;
        it->bounds = sf::FloatRect(std::floor(right), kButtonY, width, kButtonHeight);
        right -= kButtonGap;
    }
    if (!group.empty()) m_captions.push_back({ group, right + kButtonGap });
}

int Toolbar::ButtonAt(sf::Vector2f pos) const {
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i].bounds.contains(pos)) return static_cast<int>(i);
    }
    return -1;
}

bool Toolbar::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    (void)window;
    if (event.type == sf::Event::MouseMoved) {
        m_mousePos = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
        return false;
    }
    if (event.type == sf::Event::MouseLeft) {
        m_mousePos = { -1.0f, -1.0f };
        return false;
    }
    if (event.type != sf::Event::MouseButtonPressed) return false;

    sf::Vector2f pos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
    if (pos.y > Theme::ToolbarHeight) return false;

    // Anything on the bar belongs to the bar, so a miss never reaches the canvas.
    if (event.mouseButton.button == sf::Mouse::Left) {
        int index = ButtonAt(pos);
        if (index >= 0 && Enabled(m_buttons[index]) && m_buttons[index].action.onClick) {
            m_hoverTime = 0.0f;
            m_buttons[index].action.onClick();
        }
    }
    return true;
}

void Toolbar::Render(sf::RenderWindow& window) {
    Layout();

    const float dt = std::min(0.1f, m_clock.restart().asSeconds());
    int hovered = ButtonAt(m_mousePos);
    if (hovered != m_hoveredIndex) {
        m_hoveredIndex = hovered;
        m_hoverTime = 0.0f;
    } else if (hovered >= 0) {
        m_hoverTime += dt;
    }

    sf::RectangleShape background({ m_bounds.width, Theme::ToolbarHeight });
    background.setPosition(m_bounds.left, 0.0f);
    background.setFillColor(Theme::PanelBackground);
    window.draw(background);

    sf::RectangleShape border({ m_bounds.width, Theme::BorderThickness });
    border.setPosition(m_bounds.left, Theme::ToolbarHeight - Theme::BorderThickness);
    border.setFillColor(Theme::BorderColor);
    window.draw(border);

    for (const auto& caption : m_captions) {
        sf::Text text(caption.text, m_font, kCaptionSize);
        text.setFillColor(Theme::TextMuted);
        text.setLetterSpacing(1.6f);
        text.setPosition(std::floor(caption.x + 2.0f), kCaptionY);
        window.draw(text);
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        Button& b = m_buttons[i];
        const bool enabled = Enabled(b);
        const bool active = enabled && b.action.isActive && b.action.isActive();
        const bool isHovered = enabled && static_cast<int>(i) == m_hoveredIndex;

        float target = isHovered ? 1.0f : 0.0f;
        b.hover += (target - b.hover) * std::min(1.0f, dt * 14.0f);

        sf::Color fill = MixColor(Theme::HoverColor, Theme::BevelLight, b.hover * 0.5f);
        sf::Color outline = Theme::BorderColor;
        sf::Color textColor = Theme::TextPrimary;

        if (!enabled) {
            fill = Theme::PanelBackground;
            textColor = Theme::TextMuted;
        } else if (b.action.isPrimary && b.action.isPrimary()) {
            fill = MixColor(Theme::AccentColor, Theme::AccentHoverColor, b.hover);
            outline = fill;
            textColor = sf::Color(10, 18, 30);
        } else if (active) {
            fill = MixColor(Theme::PanelBackground, Theme::AccentColor, 0.32f);
            outline = Theme::AccentColor;
            textColor = sf::Color::White;
        }

        DrawRoundedRect(window, b.bounds, Theme::CornerRadius, fill, outline, 1.0f);

        sf::Text label(b.action.label, m_font, kLabelSize);
        label.setFillColor(textColor);
        sf::FloatRect tb = label.getLocalBounds();
        label.setPosition(std::floor(b.bounds.left + (b.bounds.width - tb.width) / 2.0f - tb.left),
                          std::floor(b.bounds.top + (b.bounds.height - tb.height) / 2.0f - tb.top));
        window.draw(label);
    }

    // Tooltip after a short hover.
    if (m_hoveredIndex >= 0 && m_hoverTime > 0.35f && !m_buttons[m_hoveredIndex].action.tooltip.empty()) {
        const Button& b = m_buttons[m_hoveredIndex];
        sf::Text tip(b.action.tooltip, m_font, 12);
        sf::FloatRect tb = tip.getLocalBounds();
        float width = tb.width + 20.0f;
        float height = 26.0f;
        float x = std::min(b.bounds.left, m_bounds.left + m_bounds.width - width - 8.0f);
        x = std::max(m_bounds.left + 8.0f, x);
        float y = Theme::ToolbarHeight + 6.0f;

        DrawRoundedRect(window, { std::floor(x), y, width, height }, 5.0f, sf::Color(12, 13, 16, 245), Theme::BorderColor, 1.0f);
        tip.setFillColor(Theme::TextPrimary);
        tip.setPosition(std::floor(x + 10.0f - tb.left), std::floor(y + (height - tb.height) / 2.0f - tb.top));
        window.draw(tip);
    }
}

}
