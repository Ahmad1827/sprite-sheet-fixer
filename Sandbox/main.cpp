#include <SFML/Graphics.hpp>
#include "SpriteSheetStudioPanel.h"

int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Sprite Sheet Studio — Sandbox");
    window.setFramerateLimit(60);

    // Instantiate the embeddable editor module
    StudioUI::SpriteSheetStudioPanel editorPanel;
    editorPanel.Initialize();
    
    // Set initial bounds to the full window
    sf::Vector2u size = window.getSize();
    editorPanel.SetBounds(sf::FloatRect(0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y)));
    editorPanel.OnActivated();

    sf::Clock clock;

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
                // Notify the panel of its new bounds
                editorPanel.SetBounds(visibleArea);
            }

            // Feed events to the editor module
            editorPanel.HandleEvent(event, window);
        }

        editorPanel.Update(deltaTime);

        window.clear(sf::Color(32, 34, 38)); // StudioUI::Theme::MainBackground
        
        // Render the editor module
        editorPanel.Render(window);
        
        window.display();
    }

    editorPanel.OnDeactivated();
    return 0;
}