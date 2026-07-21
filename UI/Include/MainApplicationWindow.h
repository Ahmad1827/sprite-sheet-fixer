#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "StudioEngineFacade.h"
#include "Panels/PreviewViewport.h"

class MainApplicationWindow {
public:
    MainApplicationWindow();
    void Run();

    // Helper to trigger image loading programmatically or via UI keybind
    bool LoadImage(const std::string& filePath);

private:
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    sf::RenderWindow m_window;
    StudioCore::StudioEngineFacade m_engine;
    PreviewViewport m_viewport;
};