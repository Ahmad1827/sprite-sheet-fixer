#include "MainApplicationWindow.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/Project.h"
#include "Panels/AnimationPanel.h"
#include <iostream>
#include "Utils/NativeFileDialog.h"
#include "Panels/Toolbar.h"

MainApplicationWindow::MainApplicationWindow() 
    : m_window(sf::VideoMode(1280, 720), "Sprite Sheet Studio - Milestone 10") {
    
    m_window.setFramerateLimit(60);
    m_engine.Initialize();
    m_engine.CreateProject();
    
    m_viewport.Initialize();
    
    // Wire up Toolbar with callbacks to the Main Window
    m_toolbar.Initialize("Resources/font.ttf", 
        [this]() { 
            std::string path = StudioUI::NativeFileDialog::OpenFileDialog();
            if (!path.empty()) LoadImage(path); 
        },
        [this]() { 
            std::string path = StudioUI::NativeFileDialog::OpenFileDialog("Sprite Sheet Studio Project (*.sps)");
            if (!path.empty()) {
                std::string err;
                if (m_engine.LoadProject(path, err)) {
                    m_viewport.RefreshTexture(m_engine);
                    std::cout << "[✓] Project loaded." << std::endl;
                }
            }
        },
        [this]() { 
            m_isUIHidden = !m_isUIHidden;
            m_viewport.SetUIHidden(m_isUIHidden);
        },
        [this]() {
            if (!m_engine.IsProjectActive() || !m_engine.GetCurrentProject() || m_engine.GetCurrentProject()->GetSprites().empty()) return;
            m_isWizardMode = true;
            m_animBuilderPanel.Activate(m_engine);
        }
    );
    
    m_animationPanel = std::make_unique<StudioUI::AnimationPanel>();
    m_animationPanel->InitializeFont("Resources/font.ttf");
    m_exportPreview.InitializeFont("Resources/font.ttf");
    m_animBuilderPanel.InitializeFont("Resources/font.ttf");
}

MainApplicationWindow::~MainApplicationWindow() = default;

bool MainApplicationWindow::LoadImage(const std::string& filePath) {
    std::string errorMsg;
    if (m_engine.ImportImage(filePath, errorMsg)) {
        m_viewport.RefreshTexture(m_engine);
        return true;
    }
    return false;
}

void MainApplicationWindow::Run() {
    sf::Clock clock;
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

        if (m_isWizardMode) {
            bool exitWizard = false;
            m_animBuilderPanel.HandleEvent(event, m_window, m_engine, exitWizard);
            if (exitWizard) {
                m_isWizardMode = false;
            }
            continue;
        }

        if (!m_isExportMode) {
            if (m_toolbar.HandleEvent(event, m_window, m_engine)) {
                continue;
            }
        }

        if (m_isExportMode) {
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                m_isExportMode = false;
            } else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
                std::string savePath = StudioUI::NativeFileDialog::SaveFileDialog("aligned_spritesheet.png");
                if (!savePath.empty()) {
                    if (m_engine.ExportPNG(savePath, 8)) {
                        std::cout << "[✓] Exported atlas and metadata successfully to: " << savePath << std::endl;
                        m_isExportMode = false;
                    } else {
                        std::cerr << "[X] Failed to save exported image." << std::endl;
                    }
                }
            } else {
                m_exportPreview.HandleEvent(event, m_window, m_engine);
            }
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::E && event.key.control) {
            m_isExportMode = true;
            m_exportPreview.Activate(m_engine);
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::S && event.key.control) {
            std::string path = StudioUI::NativeFileDialog::SaveFileDialog("project.sps");
            if (!path.empty()) {
                if (path.find(".sps") == std::string::npos) path += ".sps";
                if (m_engine.SaveProject(path)) {
                    std::cout << "[✓] Project saved to " << path << std::endl;
                } else {
                    std::cerr << "[X] Failed to save project." << std::endl;
                }
            }
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::L && event.key.control) {
            if (m_engine.HasTexture() || m_engine.IsProjectActive()) {
                std::cout << "[!] A project or image is already active. Clear or restart to load a new project." << std::endl;
                continue;
            }
            std::string path = StudioUI::NativeFileDialog::OpenFileDialog("Sprite Sheet Studio Project (*.sps)");
            if (!path.empty()) {
                std::string error;
                if (m_engine.LoadProject(path, error)) {
                    m_viewport.RefreshTexture(m_engine);
                    std::cout << "[✓] Project loaded from " << path << std::endl;
                } else {
                    std::cerr << "[X] Failed to load project: " << error << std::endl;
                }
            }
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::O) {
            if (m_engine.HasTexture()) {
                std::cout << "[!] An image is already loaded. Clear or restart to open a new one." << std::endl;
                continue;
            }
            std::string filePath = StudioUI::NativeFileDialog::OpenFileDialog();
            if (!filePath.empty()) {
                LoadImage(filePath);
            }
            continue;
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
    if (!m_isExportMode && !m_isWizardMode) {
        m_engine.Update(deltaTime);
        m_viewport.Update(deltaTime);
    }
}

void MainApplicationWindow::Render() {
    m_window.clear(sf::Color(30, 30, 30));
    
    if (m_isExportMode) {
        m_exportPreview.Render(m_window);
    } else {
        m_viewport.Render(m_window, m_engine);
        
        if (!m_isUIHidden && m_animationPanel) {
            m_animationPanel->Render(m_window, m_engine);
        }
        m_toolbar.Render(m_window);

        if (m_isWizardMode) {
            m_animBuilderPanel.Render(m_window);
        }
    }
    
    m_window.display();
}