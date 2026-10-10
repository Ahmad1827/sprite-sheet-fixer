#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>
#include "AppSettings.h"

namespace StudioUI {

class SettingsPanel {
public:
    void InitializeFont(const std::string& fontPath);
    void Open(AppSettings& settings);
    bool IsOpen() const { return m_settings != nullptr; }
    void SetBounds(const sf::FloatRect& bounds) { m_bounds = bounds; }

    // Returns true while the window is open (it takes every event).
    bool HandleEvent(const sf::Event& event);
    void Render(sf::RenderWindow& window);

private:
    // The window is drawn and hit-tested from the same pass: drawing a control
    // records its rectangle and what a click on it does.
    struct Hit {
        sf::FloatRect rect;
        std::function<void()> onClick;
    };

    void Close();
    void Section(sf::RenderWindow& window, const std::string& title, float& y);
    void Label(sf::RenderWindow& window, const std::string& text, const std::string& note, float y);
    void Choice(sf::RenderWindow& window, float& x, float y, const std::string& label, bool selected, std::function<void()> onClick, bool enabled = true);
    void TextField(sf::RenderWindow& window, const sf::FloatRect& rect, std::string& value, int fieldId, const std::string& placeholder, bool enabled = true);
    void Button(sf::RenderWindow& window, const sf::FloatRect& rect, const std::string& label, bool primary, std::function<void()> onClick);
    sf::FloatRect ModalRect() const;

    AppSettings* m_settings{nullptr};
    sf::Font m_font;
    sf::FloatRect m_bounds{0.0f, 0.0f, 1280.0f, 720.0f};
    sf::Vector2f m_mousePos{-1.0f, -1.0f};
    std::vector<Hit> m_hits;
    int m_focusedField{0};          // 0 = none
    std::string* m_focusedValue{nullptr};
    std::string m_testResult;
    bool m_testOk{false};
    sf::Clock m_caretClock;
};

}
