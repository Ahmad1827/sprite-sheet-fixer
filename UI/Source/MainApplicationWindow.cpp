#include "MainApplicationWindow.h"
#include "DataModels/SourceTexture.h"
#include <iostream>

MainApplicationWindow::MainApplicationWindow() 
    : m_window(sf::VideoMode(1280, 720), "Sprite Sheet Studio - Milestone 3") {
    
    m_window.setFramerateLimit(60);
    m_engine.Initialize();
    m_engine.CreateProject();
    
    // Call Initialize here instead of LoadFont
    m_viewport.Initialize();
}

bool MainApplicationWindow::LoadImage(const std::string& filePath) {
    std::string errorMsg;
    if (m_engine.ImportImage(filePath, errorMsg)) {
        std::cout << "[✓] Successfully imported: " << filePath << std::endl;
        auto tex = m_engine.GetCurrentTexture();
        if (tex) {
            std::cout << "    Dimensions: " << tex->GetWidth() << "x" << tex->GetHeight() << " px\n";
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
    std::cout << "  Sprite Sheet Studio - Milestone 3 Controls" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  [O] Load PNG via Terminal" << std::endl;
    std::cout << "  [G] Toggle Grid Overlay" << std::endl;
    std::cout << "  [F3] Toggle Debug HUD" << std::endl;
    std::cout << "  [F] Fit Image to Window" << std::endl;
    std::cout << "  [C] Center View" << std::endl;
    std::cout << "  [R] Reset Zoom" << std::endl;
    std::cout << "  [Shift + R] Reset Pan (Snap to 0,0)" << std::endl;
    std::cout << "  [Z] Zoom to Selection Box" << std::endl;
    std::cout << "  Left Click + Drag: Draw Selection Rect" << std::endl;
    std::cout << "  Left Double-Click: Center on Point" << std::endl;
    std::cout << "  Middle Mouse Drag: Pan" << std::endl;
    std::cout << "  Mouse Wheel: Zoom" << std::endl;
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
        } 
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::O) {
            std::cout << "\nEnter PNG file path: ";
            std::string filePath;
            if (std::cin >> filePath) {
                LoadImage(filePath);
            }
        } 
        else {
            m_viewport.HandleEvent(event, m_window, m_engine);
        }
    }
}

void MainApplicationWindow::Update(float deltaTime) {
    m_viewport.Update(deltaTime);
}

void MainApplicationWindow::Render() {
    m_window.clear(sf::Color(30, 30, 30));
    m_viewport.Render(m_window, m_engine);
    m_window.display();
}