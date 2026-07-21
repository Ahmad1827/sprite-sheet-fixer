#pragma once
#include <SFML/Graphics.hpp>
#include <memory>

namespace StudioCore {
    class StudioEngineFacade;
}

class PreviewViewport {
public:
    PreviewViewport();

    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window);
    void Update(float deltaTime);
    void Render(sf::RenderWindow& window);
    void RefreshTexture(const StudioCore::StudioEngineFacade& engine);
    void ResetView();

private:
    sf::Texture m_gpuTexture;
    sf::Sprite m_sprite;
    sf::View m_view;
    
    bool m_isPanning{false};
    sf::Vector2i m_lastMousePos;
    bool m_hasValidTexture{false};
};