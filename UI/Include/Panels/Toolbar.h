#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

namespace StudioCore { class StudioEngineFacade; }

namespace StudioUI {

class Toolbar {
public:
    Toolbar();
    void Initialize(const std::string& fontPath, 
                    std::function<void()> onOpenImage, 
                    std::function<void()> onLoadProject,
                    std::function<void()> onToggleUI,
                    std::function<void()> onOpenWizard);
    
    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine);
    void Render(sf::RenderWindow& window);

private:
    struct Button {
        sf::RectangleShape rect;
        sf::Text text;
        std::function<void(StudioCore::StudioEngineFacade&)> action;
    };
    std::vector<Button> m_buttons;
    sf::Font m_font;
    sf::RectangleShape m_background;

    void AddButton(float x, const std::string& label, std::function<void(StudioCore::StudioEngineFacade&)> action);
};

}