#include "Panels/SettingsPanel.h"
#include "AI/ClaudeReview.h"
#include "Theme.h"
#include "Widgets.h"
#include <algorithm>
#include <cmath>

namespace StudioUI {

namespace {
constexpr float kWidth = 700.0f;
constexpr float kHeight = 596.0f;
constexpr float kPadding = 28.0f;
constexpr float kControlX = 210.0f;   // where the choices start, from the left edge of the window
constexpr float kRowHeight = 42.0f;
constexpr float kChipHeight = 28.0f;
}

void SettingsPanel::InitializeFont(const std::string& fontPath) {
    LoadUiFont(m_font, fontPath);
}

void SettingsPanel::Open(AppSettings& settings) {
    m_settings = &settings;
    m_focusedField = 0;
    m_focusedValue = nullptr;
    m_testResult.clear();
}

void SettingsPanel::Close() {
    if (m_settings) m_settings->Save();
    m_settings = nullptr;
    m_focusedField = 0;
    m_focusedValue = nullptr;
    m_hits.clear();
}

sf::FloatRect SettingsPanel::ModalRect() const {
    const float width = std::min(kWidth, m_bounds.width - 24.0f);
    const float height = std::min(kHeight, m_bounds.height - 24.0f);
    return sf::FloatRect(std::floor(m_bounds.left + (m_bounds.width - width) / 2.0f),
                         std::floor(m_bounds.top + (m_bounds.height - height) / 2.0f), width, height);
}

bool SettingsPanel::HandleEvent(const sf::Event& event) {
    if (!IsOpen()) return false;

    if (event.type == sf::Event::MouseMoved) {
        m_mousePos = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
    } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f pos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
        m_focusedField = 0;
        m_focusedValue = nullptr;
        if (!ModalRect().contains(pos)) {
            Close();
            return true;
        }
        // Copy first: a click may close the window and clear the list.
        std::vector<Hit> hits = m_hits;
        for (const auto& hit : hits) {
            if (hit.rect.contains(pos)) {
                if (hit.onClick) hit.onClick();
                break;
            }
        }
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Enter) {
            if (m_focusedField != 0) {
                m_focusedField = 0;
                m_focusedValue = nullptr;
            } else {
                Close();
            }
        }
    } else if (event.type == sf::Event::TextEntered && m_focusedValue) {
        const sf::Uint32 c = event.text.unicode;
        if (c == 8) {
            if (!m_focusedValue->empty()) m_focusedValue->pop_back();
        } else if (c == 22) {
            // Ctrl+V
            for (sf::Uint32 ch : sf::Clipboard::getString()) {
                if (ch >= 32 && ch < 127 && m_focusedValue->size() < 300) m_focusedValue->push_back(static_cast<char>(ch));
            }
        } else if (c >= 32 && c < 127 && m_focusedValue->size() < 300) {
            m_focusedValue->push_back(static_cast<char>(c));
        }
        m_caretClock.restart();
    }
    return true;
}

void SettingsPanel::Section(sf::RenderWindow& window, const std::string& title, float& y) {
    const sf::FloatRect modal = ModalRect();
    sf::Text text(title, m_font, 11);
    text.setFillColor(Theme::TextAccent);
    text.setLetterSpacing(1.8f);
    text.setPosition(modal.left + kPadding, y);
    window.draw(text);

    sf::RectangleShape rule({ modal.width - kPadding * 2.0f, 1.0f });
    rule.setPosition(modal.left + kPadding, y + 20.0f);
    rule.setFillColor(Theme::BorderColor);
    window.draw(rule);
    y += 32.0f;
}

void SettingsPanel::Label(sf::RenderWindow& window, const std::string& text, const std::string& note, float y) {
    const sf::FloatRect modal = ModalRect();
    sf::Text label(text, m_font, 13);
    label.setFillColor(Theme::TextPrimary);
    label.setPosition(modal.left + kPadding, y + (note.empty() ? 6.0f : 0.0f));
    window.draw(label);

    if (!note.empty()) {
        sf::Text small(note, m_font, 10);
        small.setFillColor(Theme::TextMuted);
        small.setPosition(modal.left + kPadding, y + 18.0f);
        window.draw(small);
    }
}

void SettingsPanel::Choice(sf::RenderWindow& window, float& x, float y, const std::string& label, bool selected,
                           std::function<void()> onClick, bool enabled) {
    sf::Text text(label, m_font, 12);
    sf::FloatRect tb = text.getLocalBounds();
    const sf::FloatRect rect(std::floor(x), y, std::ceil(tb.width) + 26.0f, kChipHeight);
    const bool hovered = enabled && rect.contains(m_mousePos);

    sf::Color fill = selected ? MixColor(Theme::PanelBackground, Theme::AccentColor, 0.32f) : Theme::HoverColor;
    if (hovered) fill = MixColor(fill, sf::Color::White, 0.08f);
    sf::Color outline = selected ? Theme::AccentColor : Theme::BorderColor;
    sf::Color textColor = selected ? sf::Color::White : Theme::TextSecondary;
    if (!enabled) {
        fill = Theme::InspectorBackground;
        outline = Theme::BorderColor;
        textColor = Theme::TextMuted;
    }

    DrawRoundedRect(window, rect, 5.0f, fill, outline, 1.0f);
    text.setFillColor(textColor);
    text.setPosition(std::floor(rect.left + (rect.width - tb.width) / 2.0f - tb.left),
                     std::floor(rect.top + (rect.height - tb.height) / 2.0f - tb.top));
    window.draw(text);

    if (enabled) m_hits.push_back({ rect, std::move(onClick) });
    x += rect.width + 6.0f;
}

void SettingsPanel::TextField(sf::RenderWindow& window, const sf::FloatRect& rect, std::string& value, int fieldId,
                              const std::string& placeholder, bool enabled) {
    const bool focused = enabled && m_focusedField == fieldId;
    DrawRoundedRect(window, rect, 5.0f, sf::Color(14, 15, 18), focused ? Theme::AccentColor : Theme::BorderColor, 1.0f);

    sf::Text text(value.empty() && !focused ? placeholder : value, m_font, 12);
    text.setFillColor(!enabled ? Theme::TextMuted : (value.empty() && !focused ? Theme::TextMuted : Theme::TextPrimary));

    // Keep the end of long text in view.
    std::string shown = text.getString();
    while (shown.size() > 1 && text.getLocalBounds().width > rect.width - 20.0f) {
        shown.erase(shown.begin());
        text.setString(shown);
    }
    text.setPosition(std::floor(rect.left + 10.0f), std::floor(rect.top + 6.0f));
    window.draw(text);

    if (focused && std::fmod(m_caretClock.getElapsedTime().asSeconds(), 1.0f) < 0.55f) {
        sf::RectangleShape caret({ 1.0f, rect.height - 12.0f });
        caret.setPosition(std::floor(rect.left + 11.0f + (value.empty() ? 0.0f : text.getLocalBounds().width + 1.0f)), rect.top + 6.0f);
        caret.setFillColor(Theme::TextPrimary);
        window.draw(caret);
    }

    if (enabled) {
        std::string* target = &value;
        m_hits.push_back({ rect, [this, fieldId, target]() {
            m_focusedField = fieldId;
            m_focusedValue = target;
            m_caretClock.restart();
        } });
    }
}

void SettingsPanel::Button(sf::RenderWindow& window, const sf::FloatRect& rect, const std::string& label, bool primary,
                           std::function<void()> onClick) {
    const bool hovered = rect.contains(m_mousePos);
    sf::Color fill = primary ? (hovered ? Theme::AccentHoverColor : Theme::AccentColor)
                             : (hovered ? Theme::BevelLight : Theme::HoverColor);
    DrawRoundedRect(window, rect, Theme::CornerRadius, fill, primary ? fill : Theme::BorderColor, 1.0f);
    sf::Text text(label, m_font, 13);
    text.setFillColor(primary ? sf::Color(10, 18, 30) : Theme::TextPrimary);
    sf::FloatRect tb = text.getLocalBounds();
    text.setPosition(std::floor(rect.left + (rect.width - tb.width) / 2.0f - tb.left),
                     std::floor(rect.top + (rect.height - tb.height) / 2.0f - tb.top));
    window.draw(text);
    m_hits.push_back({ rect, std::move(onClick) });
}

void SettingsPanel::Render(sf::RenderWindow& window) {
    if (!IsOpen()) return;
    AppSettings& s = *m_settings;
    m_hits.clear();

    sf::RectangleShape dim({ m_bounds.width, m_bounds.height });
    dim.setPosition(m_bounds.left, m_bounds.top);
    dim.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(dim);

    const sf::FloatRect modal = ModalRect();
    DrawRoundedRect(window, modal, 12.0f, Theme::PanelBackground, Theme::BorderColor, 1.0f);

    sf::Text title("Settings", m_font, 20);
    title.setFillColor(Theme::TextPrimary);
    title.setPosition(modal.left + kPadding, modal.top + 20.0f);
    window.draw(title);

    const float controlX = modal.left + kControlX;
    const float fieldWidth = modal.left + modal.width - kPadding - controlX;
    float y = modal.top + 64.0f;
    float x = 0.0f;

    // ---- Clean -------------------------------------------------------------
    Section(window, "CLEAN", y);

    Label(window, "Edges", "how sprite outlines are cut", y);
    x = controlX;
    Choice(window, x, y + 2.0f, "Soft", s.cleanSoftEdges, [&s]() { s.cleanSoftEdges = true; });
    Choice(window, x, y + 2.0f, "Hard pixels", !s.cleanSoftEdges, [&s]() { s.cleanSoftEdges = false; });
    y += kRowHeight;

    Label(window, "Frame lines", "lines drawn between frames", y);
    x = controlX;
    Choice(window, x, y + 2.0f, "Remove", s.cleanRemoveFrameLines, [&s]() { s.cleanRemoveFrameLines = true; });
    Choice(window, x, y + 2.0f, "Keep", !s.cleanRemoveFrameLines, [&s]() { s.cleanRemoveFrameLines = false; });
    y += kRowHeight;

    Label(window, "Small leftovers", "loose bits Clean also deletes", y);
    x = controlX;
    const int sizes[4] = { 0, 8, 40, 200 };
    const char* names[4] = { "Keep all", "Tiny", "Small", "Large" };
    for (int i = 0; i < 4; ++i) {
        const int size = sizes[i];
        Choice(window, x, y + 2.0f, names[i], s.cleanSpeckSize == size, [&s, size]() { s.cleanSpeckSize = size; });
    }
    y += kRowHeight + 6.0f;

    // ---- Claude Code -------------------------------------------------------
    Section(window, "AI REVIEW  (CLAUDE CODE)", y);

    Label(window, "AI Review button", "needs Claude Code installed", y);
    x = controlX;
    Choice(window, x, y + 2.0f, "On", s.aiEnabled, [&s]() { s.aiEnabled = true; });
    Choice(window, x, y + 2.0f, "Off", !s.aiEnabled, [&s]() { s.aiEnabled = false; });
    y += kRowHeight;

    const bool ai = s.aiEnabled;

    Label(window, "Command", "how Claude Code is started here", y);
    TextField(window, { controlX, y + 2.0f, fieldWidth - 76.0f, kChipHeight }, s.aiCommand, 1, "claude", ai);
    if (ai) {
        Button(window, { controlX + fieldWidth - 68.0f, y + 2.0f, 68.0f, kChipHeight }, "Test", false, [this, &s]() {
            const std::string version = ClaudeReview::TestCommand(s.aiCommand.empty() ? "claude" : s.aiCommand);
            m_testOk = !version.empty();
            m_testResult = m_testOk ? "Found: " + version : "Not found. Install Claude Code, or type the full command.";
        });
    }
    y += kRowHeight - 8.0f;
    if (!m_testResult.empty()) {
        sf::Text result(m_testResult, m_font, 11);
        result.setFillColor(m_testOk ? Theme::SuccessColor : Theme::DangerColor);
        result.setPosition(controlX, y);
        window.draw(result);
    }
    y += 22.0f;

    Label(window, "Model", "faster or more careful", y);
    x = controlX;
    const char* models[4] = { "default", "haiku", "sonnet", "opus" };
    const char* modelNames[4] = { "Default", "Haiku", "Sonnet", "Opus" };
    for (int i = 0; i < 4; ++i) {
        const std::string model = models[i];
        Choice(window, x, y + 2.0f, modelNames[i], s.aiModel == model, [&s, model]() { s.aiModel = model; }, ai);
    }
    y += kRowHeight;

    Label(window, "Look for", "what Claude should mark", y);
    x = controlX;
    Choice(window, x, y + 2.0f, "Text labels", s.aiRemoveLabels, [&s]() { s.aiRemoveLabels = !s.aiRemoveLabels; }, ai);
    Choice(window, x, y + 2.0f, "Stray bits", s.aiRemoveDebris, [&s]() { s.aiRemoveDebris = !s.aiRemoveDebris; }, ai);
    y += kRowHeight;

    Label(window, "Before deleting", "", y);
    x = controlX;
    Choice(window, x, y + 2.0f, "Show me first", s.aiAskFirst, [&s]() { s.aiAskFirst = true; }, ai);
    Choice(window, x, y + 2.0f, "Delete right away", !s.aiAskFirst, [&s]() { s.aiAskFirst = false; }, ai);
    y += kRowHeight;

    Label(window, "Extra instructions", "anything else to tell Claude", y);
    TextField(window, { controlX, y + 2.0f, fieldWidth, kChipHeight }, s.aiExtraInstructions, 2,
              "for example: also remove the shadows under the characters", ai);

    Button(window, { modal.left + modal.width - kPadding - 110.0f, modal.top + modal.height - 52.0f, 110.0f, 34.0f },
           "Done", true, [this]() { Close(); });

    sf::Text saved("Saved automatically to settings.json", m_font, 11);
    saved.setFillColor(Theme::TextMuted);
    saved.setPosition(modal.left + kPadding, modal.top + modal.height - 40.0f);
    window.draw(saved);
}

}
