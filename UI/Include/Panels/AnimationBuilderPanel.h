#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "Systems/AnimationBuilder.h"

namespace StudioCore { class StudioEngineFacade; }

namespace StudioUI {

class AnimationBuilderPanel {
public:
    AnimationBuilderPanel();
    void InitializeFont(const std::string& fontPath);
    void Activate(StudioCore::StudioEngineFacade& engine);
    bool IsActive() const { return m_isActive; }
    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, bool& exitWizard);
    void Render(sf::RenderWindow& window);

private:
    bool m_isActive{false};
    sf::Font m_font;
    std::vector<StudioCore::ProposedAnimation> m_proposals;
    
    sf::RectangleShape m_background;
    sf::RectangleShape m_modalBackground;
};

}