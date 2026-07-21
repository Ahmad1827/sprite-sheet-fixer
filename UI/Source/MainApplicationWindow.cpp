#include "MainApplicationWindow.h"
#include "DataModels/SourceTexture.h"
#include "Panels/AnimationPanel.h"
#include <iostream>

MainApplicationWindow::MainApplicationWindow() 
    : m_window(sf::VideoMode(1280, 720), "Sprite Sheet Studio - Milestone 6") {
    
    m_window.setFramerateLimit(60);
    m_engine.Initialize();
    m_engine.CreateProject();
    
    m_viewport.Initialize();
    
    m_animationPanel = std::make_unique<StudioUI::AnimationPanel>();
    
    // Uses the robust system fallback mechanism
    m_animationPanel->InitializeFont("Resources/font.ttf");
}

MainApplicationWindow::~MainApplicationWindow() = default;

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
    std::cout << "  Sprite Sheet Studio - Milestone 6 Controls" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  [O] Load PNG via Terminal" << std::endl;
    std::cout << "  [Ctrl + D] Auto Detect Sprites (CCL)" << std::endl;
    std::cout << "  [Shift + N] Create New Animation Group" << std::endl;
    std::cout << "  [Space] Play / Pause Active Animation" << std::endl;
    std::cout << "  [Left Click] Select Sprite" << std::endl;
    std::cout << "  [Shift + Click] Add Sprite to Selection" << std::endl;
    std::cout << "  [Alt + Drag] Edit Pivot" << std::endl;
    std::cout << "  [Ctrl + Alt + Drag] Edit Baseline" << std::endl;
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
            
            if (m_animationPanel) {
                m_animationPanel->HandleEvent(event, m_window, m_engine, m_viewport);
            }
        }
    }
}

void MainApplicationWindow::Update(float deltaTime) {
    m_engine.Update(deltaTime);
    m_viewport.Update(deltaTime);
}

void MainApplicationWindow::Render() {
    m_window.clear(sf::Color(30, 30, 30));
    m_viewport.Render(m_window, m_engine);
    if (m_animationPanel) {
        m_animationPanel->Render(m_window, m_engine);
    }
    m_window.display();
}