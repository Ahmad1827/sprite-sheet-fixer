#include "MainApplicationWindow.h"
#include "DataModels/SourceTexture.h"
#include <iostream>

MainApplicationWindow::MainApplicationWindow() 
    : m_window(sf::VideoMode(1280, 720), "Sprite Sheet Studio - Milestone 2") {
    
    m_window.setFramerateLimit(60);
    m_engine.Initialize();
    m_engine.CreateProject();
}

bool MainApplicationWindow::LoadImage(const std::string& filePath) {
    std::string errorMsg;
    if (m_engine.ImportImage(filePath, errorMsg)) {
        std::cout << "[✓] Successfully imported: " << filePath << std::endl;
        auto tex = m_engine.GetCurrentTexture();
        if (tex) {
            std::cout << "    Dimensions: " << tex->GetWidth() << "x" << tex->GetHeight() << " px" << std::endl;
            std::cout << "    Pixel Buffer Size: " << tex->GetPixels().size() << " bytes" << std::endl;
        }
        m_viewport.RefreshTexture(m_engine);
        return true;
    } else {
        std::cerr << "[X] Failed to import: " << errorMsg << std::endl;
        return false;
    }
}

void MainApplicationWindow::Run() {
    sf::Clock clock;
    std::cout << "\n==================================================" << std::endl;
    std::cout << "  Sprite Sheet Studio - Milestone 2 Viewport" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << " Controls:" << std::endl;
    std::cout << "  - Press [O] to open file prompt in terminal" << std::endl;
    std::cout << "  - Mouse Wheel: Zoom in / out" << std::endl;
    std::cout << "  - Middle Mouse Drag: Pan viewport" << std::endl;
    std::cout << "==================================================\n" << std::endl;

    while (m_window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        ProcessEvents();
        Update(deltaTime);
        Render();
    }
}

void MainApplicationWindow::ProcessEvents() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            m_window.close();
        } else if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::O) {
                std::cout << "\nEnter PNG file path: ";
                std::string filePath;
                if (std::cin >> filePath) {
                    LoadImage(filePath);
                }
            }
        } else {
            m_viewport.HandleEvent(event, m_window);
        }
    }
}

void MainApplicationWindow::Update(float deltaTime) {
    m_viewport.Update(deltaTime);
}

void MainApplicationWindow::Render() {
    m_window.clear(sf::Color(30, 30, 30));
    m_viewport.Render(m_window);
    m_window.display();
}