#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "Systems/AnimationBuilder.h"

namespace StudioCore { class StudioEngineFacade; }

namespace StudioUI {

class AnimationBuilderPanel {
public:
    AnimationBuilderPanel() = default;
    void InitializeFont(const std::string& fontPath);
    void Activate(StudioCore::StudioEngineFacade& engine);
    bool IsActive() const { return m_isActive; }
    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, bool& exitWizard);
    void SetBounds(const sf::FloatRect& bounds) { m_bounds = bounds; }
    void Render(sf::RenderWindow& window);

private:
    // Every rectangle is derived from the same layout, so what is drawn is
    // exactly what can be clicked.
    sf::FloatRect ModalRect() const;
    sf::FloatRect RowRect(size_t index) const;
    sf::FloatRect FpsRect(size_t index) const;
    sf::FloatRect LoopRect(size_t index) const;
    sf::FloatRect ReverseRect(size_t index) const;
    sf::FloatRect CreateRect() const;
    sf::FloatRect CancelRect() const;
    size_t VisibleRows() const;

    bool m_isActive{false};
    sf::Font m_font;
    std::vector<StudioCore::ProposedAnimation> m_proposals;
    sf::FloatRect m_bounds{0.0f, 0.0f, 1280.0f, 720.0f};
    sf::Vector2f m_mousePos{-1.0f, -1.0f};
};

}
