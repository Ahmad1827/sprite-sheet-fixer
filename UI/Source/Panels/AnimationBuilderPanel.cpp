#include "Panels/AnimationBuilderPanel.h"
#include "StudioEngineFacade.h"
#include "Theme.h"
#include "Widgets.h"
#include <algorithm>
#include <cmath>

namespace StudioUI {

namespace {
constexpr float kModalWidth = 660.0f;
constexpr float kHeaderHeight = 86.0f;
constexpr float kRowHeight = 46.0f;
constexpr float kFooterHeight = 72.0f;
constexpr float kPadding = 24.0f;
}

void AnimationBuilderPanel::InitializeFont(const std::string& fontPath) {
    LoadUiFont(m_font, fontPath);
}

void AnimationBuilderPanel::Activate(StudioCore::StudioEngineFacade& engine) {
    m_proposals = engine.BuildAnimationsByRow();
    m_isActive = true;
}

size_t AnimationBuilderPanel::VisibleRows() const {
    const float available = m_bounds.height - 80.0f - kHeaderHeight - kFooterHeight;
    const size_t fit = static_cast<size_t>(std::max(1.0f, std::floor(available / kRowHeight)));
    return std::min(std::max<size_t>(1, m_proposals.size()), fit);
}

sf::FloatRect AnimationBuilderPanel::ModalRect() const {
    const float width = std::min(kModalWidth, m_bounds.width - 40.0f);
    const float height = kHeaderHeight + VisibleRows() * kRowHeight + kFooterHeight;
    return sf::FloatRect(std::floor(m_bounds.left + (m_bounds.width - width) / 2.0f),
                         std::floor(m_bounds.top + (m_bounds.height - height) / 2.0f), width, height);
}

sf::FloatRect AnimationBuilderPanel::RowRect(size_t index) const {
    const sf::FloatRect modal = ModalRect();
    return sf::FloatRect(modal.left + kPadding, modal.top + kHeaderHeight + index * kRowHeight,
                         modal.width - kPadding * 2.0f, kRowHeight - 8.0f);
}

sf::FloatRect AnimationBuilderPanel::ReverseRect(size_t index) const {
    const sf::FloatRect row = RowRect(index);
    return sf::FloatRect(row.left + row.width - 118.0f, row.top + 5.0f, 110.0f, row.height - 10.0f);
}

sf::FloatRect AnimationBuilderPanel::LoopRect(size_t index) const {
    const sf::FloatRect next = ReverseRect(index);
    return sf::FloatRect(next.left - 98.0f, next.top, 90.0f, next.height);
}

sf::FloatRect AnimationBuilderPanel::FpsRect(size_t index) const {
    const sf::FloatRect next = LoopRect(index);
    return sf::FloatRect(next.left - 88.0f, next.top, 80.0f, next.height);
}

sf::FloatRect AnimationBuilderPanel::CreateRect() const {
    const sf::FloatRect modal = ModalRect();
    return sf::FloatRect(modal.left + modal.width - kPadding - 210.0f, modal.top + modal.height - 52.0f, 210.0f, 34.0f);
}

sf::FloatRect AnimationBuilderPanel::CancelRect() const {
    const sf::FloatRect create = CreateRect();
    return sf::FloatRect(create.left - 100.0f, create.top, 90.0f, create.height);
}

void AnimationBuilderPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, bool& exitWizard) {
    (void)window;
    if (!m_isActive) return;

    auto commit = [&]() {
        engine.CommitProposedAnimations(m_proposals);
        m_isActive = false;
        exitWizard = true;
    };
    auto cancel = [&]() {
        m_isActive = false;
        exitWizard = true;
    };

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) cancel();
        else if (event.key.code == sf::Keyboard::Enter) commit();
        return;
    }

    if (event.type == sf::Event::MouseMoved) {
        m_mousePos = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f pos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));

        if (!ModalRect().contains(pos) || CancelRect().contains(pos)) {
            cancel();
            return;
        }
        if (CreateRect().contains(pos)) {
            if (!m_proposals.empty()) commit();
            return;
        }
        for (size_t i = 0; i < std::min(m_proposals.size(), VisibleRows()); ++i) {
            if (FpsRect(i).contains(pos)) {
                m_proposals[i].fps = (m_proposals[i].fps >= 24) ? 4 : m_proposals[i].fps + 2;
            } else if (LoopRect(i).contains(pos)) {
                m_proposals[i].isLooping = !m_proposals[i].isLooping;
            } else if (ReverseRect(i).contains(pos)) {
                m_proposals[i].reverseOrder = !m_proposals[i].reverseOrder;
            }
        }
    }
}

void AnimationBuilderPanel::Render(sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::RectangleShape dim({ m_bounds.width, m_bounds.height });
    dim.setPosition(m_bounds.left, m_bounds.top);
    dim.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(dim);

    const sf::FloatRect modal = ModalRect();
    DrawRoundedRect(window, modal, 12.0f, Theme::PanelBackground, Theme::BorderColor, 1.0f);

    sf::Text title("Animations found", m_font, 20);
    title.setFillColor(Theme::TextPrimary);
    title.setPosition(modal.left + kPadding, modal.top + 20.0f);
    window.draw(title);

    sf::Text subtitle(m_proposals.empty()
                          ? "No rows of sprites were found. Press Detect first, then try again."
                          : "One animation per row of sprites. Click a setting to change it.",
                      m_font, 12);
    subtitle.setFillColor(Theme::TextSecondary);
    subtitle.setPosition(modal.left + kPadding, modal.top + 52.0f);
    window.draw(subtitle);

    auto chip = [&](const sf::FloatRect& rect, const std::string& label, bool on) {
        const bool hovered = rect.contains(m_mousePos);
        sf::Color fill = on ? MixColor(Theme::PanelBackground, Theme::AccentColor, 0.30f) : Theme::HoverColor;
        if (hovered) fill = MixColor(fill, sf::Color::White, 0.08f);
        DrawRoundedRect(window, rect, 5.0f, fill, on ? Theme::AccentColor : Theme::BorderColor, 1.0f);
        sf::Text text(label, m_font, 12);
        text.setFillColor(on ? sf::Color::White : Theme::TextSecondary);
        sf::FloatRect b = text.getLocalBounds();
        text.setPosition(std::floor(rect.left + (rect.width - b.width) / 2.0f - b.left),
                         std::floor(rect.top + (rect.height - b.height) / 2.0f - b.top));
        window.draw(text);
    };

    const size_t rows = std::min(m_proposals.size(), VisibleRows());
    for (size_t i = 0; i < rows; ++i) {
        const auto& prop = m_proposals[i];
        const sf::FloatRect row = RowRect(i);
        DrawRoundedRect(window, row, 6.0f, Theme::InspectorBackground);

        sf::Text name(prop.name, m_font, 14);
        name.setFillColor(Theme::TextPrimary);
        name.setPosition(row.left + 14.0f, row.top + 9.0f);
        window.draw(name);

        sf::Text frames(std::to_string(prop.spriteIds.size()) + " frames", m_font, 11);
        frames.setFillColor(Theme::TextMuted);
        frames.setPosition(row.left + 14.0f + name.getLocalBounds().width + 14.0f, row.top + 13.0f);
        window.draw(frames);

        chip(FpsRect(i), std::to_string(prop.fps) + " fps", true);
        chip(LoopRect(i), prop.isLooping ? "Loops" : "Plays once", prop.isLooping);
        chip(ReverseRect(i), prop.reverseOrder ? "Reversed" : "Forward", prop.reverseOrder);
    }

    if (m_proposals.size() > rows) {
        sf::Text more("+ " + std::to_string(m_proposals.size() - rows) + " more (also created)", m_font, 11);
        more.setFillColor(Theme::TextMuted);
        more.setPosition(modal.left + kPadding, modal.top + modal.height - 42.0f);
        window.draw(more);
    }

    auto button = [&](const sf::FloatRect& rect, const std::string& label, bool primary, bool enabled) {
        const bool hovered = enabled && rect.contains(m_mousePos);
        sf::Color fill = primary ? (hovered ? Theme::AccentHoverColor : Theme::AccentColor)
                                 : (hovered ? Theme::BevelLight : Theme::HoverColor);
        if (!enabled) fill = Theme::InspectorBackground;
        DrawRoundedRect(window, rect, Theme::CornerRadius, fill, primary && enabled ? fill : Theme::BorderColor, 1.0f);
        sf::Text text(label, m_font, 13);
        text.setFillColor(!enabled ? Theme::TextMuted : (primary ? sf::Color(10, 18, 30) : Theme::TextPrimary));
        sf::FloatRect b = text.getLocalBounds();
        text.setPosition(std::floor(rect.left + (rect.width - b.width) / 2.0f - b.left),
                         std::floor(rect.top + (rect.height - b.height) / 2.0f - b.top));
        window.draw(text);
    };

    button(CancelRect(), "Cancel", false, true);
    const std::string createLabel = m_proposals.size() == 1 ? "Create 1 animation"
                                                            : "Create " + std::to_string(m_proposals.size()) + " animations";
    button(CreateRect(), createLabel, true, !m_proposals.empty());
}

}
