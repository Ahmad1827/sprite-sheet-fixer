#include "MainApplicationWindow.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/Project.h"
#include "Panels/AnimationPanel.h"
#include <iostream>
#include "Utils/NativeFileDialog.h"
#include "Panels/Toolbar.h"

MainApplicationWindow::MainApplicationWindow() 
    : m_window(sf::VideoMode(1280, 720), "Sprite Sheet Studio - Milestone 11") {
    
    m_window.setFramerateLimit(60);
    m_engine.Initialize();
    m_engine.CreateProject();
    
    m_viewport.Initialize();
    
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
    m_workspace.InitializeFont("Resources/font.ttf");
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

        // 1. KEYBOARD SHORTCUTS (CTRL+Z, CTRL+Y, etc.) - Process FIRST so UI doesn't swallow them
        if (event.type == sf::Event::KeyPressed) {
            bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || 
                             sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

            if (isControl) {
                if (event.key.code == sf::Keyboard::Z) {
                    m_engine.Undo();
                    m_viewport.RefreshTexture(m_engine);
                    continue;
                }
                if (event.key.code == sf::Keyboard::Y) {
                    m_engine.Redo();
                    m_viewport.RefreshTexture(m_engine);
                    continue;
                }
                if (event.key.code == sf::Keyboard::E) {
                    m_isExportMode = true;
                    m_exportPreview.Activate(m_engine);
                    continue;
                }
                if (event.key.code == sf::Keyboard::S) {
                    std::string path = StudioUI::NativeFileDialog::SaveFileDialog("project.sps");
                    if (!path.empty()) {
                        if (path.find(".sps") == std::string::npos) path += ".sps";
                        m_engine.SaveProject(path);
                    }
                    continue;
                }
            }
        }

        // 2. RIGHT CLICK CONTEXT MENU
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, m_window);

            std::string targetSpriteId = "";

            if (m_engine.IsProjectActive() && m_engine.GetCurrentProject()) {
                auto sprites = m_engine.GetCurrentProject()->GetSprites();
                
                for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
                    auto sprite = *it;
                    if (!sprite) continue;

                    auto rect = sprite->GetSourceRect();
                    sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
                    if (bounds.contains(worldPos)) {
                        targetSpriteId = sprite->GetId();
                        break;
                    }
                }
            }

            if (!targetSpriteId.empty()) {
                std::vector<StudioUI::ContextMenuItem> items = {
                    {"Duplicate Sprite", [this, targetSpriteId]() {
                        m_engine.DuplicateSpriteWithPixels(targetSpriteId);
                        m_viewport.RefreshTexture(m_engine);
                    }},
                    {"Delete Sprite", [this, targetSpriteId]() {
                        m_engine.DeleteSpriteWithPixels(targetSpriteId);
                        m_viewport.RefreshTexture(m_engine);
                    }},
                    {"Reset Pivot", [this, targetSpriteId]() {
                        auto proj = m_engine.GetCurrentProject();
                        if (!proj) return;
                        auto sprite = proj->GetSpriteById(targetSpriteId);
                        if (sprite) {
                            auto rect = sprite->GetSourceRect();
                            sprite->SetPivot({rect.width / 2.0f, rect.height / 2.0f});
                            m_viewport.RefreshTexture(m_engine);
                        }
                    }}
                };
                m_workspace.ShowContextMenu({static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y)}, items);
                continue;
            }
        }

        // 3. LEFT CLICK DRAG START
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            // Only initiate sprite drag if Alt is NOT pressed (Alt is reserved for pivot editing)
            if (!sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) && !sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt)) {
                sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
                sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, m_window);

                if (m_engine.IsProjectActive() && m_engine.GetCurrentProject()) {
                    auto sprites = m_engine.GetCurrentProject()->GetSprites();
                    m_draggedSprites.clear();
                    m_dragStartRects.clear();

                    for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
                        auto sprite = *it;
                        if (!sprite) continue;

                        auto rect = sprite->GetSourceRect();
                        sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);

                        if (bounds.contains(worldPos)) {
                            m_isDragging = true;
                            m_dragStartPos = worldPos;
                            m_draggedSprites.push_back(sprite);
                            m_dragStartRects.push_back({sprite->GetId(), rect});
                            break;
                        }
                    }
                }
            }
        }

        // 4. MOUSE MOVE DRAGGING
        if (event.type == sf::Event::MouseMoved && m_isDragging) {
            sf::Vector2i pixelPos(event.mouseMove.x, event.mouseMove.y);
            sf::Vector2f currentWorldPos = m_viewport.MapPixelToWorld(pixelPos, m_window);
            sf::Vector2f delta = currentWorldPos - m_dragStartPos;

            auto proj = m_engine.GetCurrentProject();
            if (proj && !m_draggedSprites.empty()) {
                for (size_t i = 0; i < m_draggedSprites.size(); ++i) {
                    auto oldSprite = m_draggedSprites[i];
                    if (!oldSprite) continue;

                    std::string id = oldSprite->GetId();
                    auto rect = oldSprite->GetSourceRect();

                    StudioCore::Rect newRect{rect.x + delta.x, rect.y + delta.y, rect.width, rect.height};
                    StudioCore::SpriteDefinition updatedSprite(id, newRect);
                    updatedSprite.SetPivot(oldSprite->GetPivot());
                    updatedSprite.SetBaseline(oldSprite->GetBaseline());

                    proj->RemoveSprite(id);
                    proj->AddSprite(updatedSprite);
                    m_draggedSprites[i] = proj->GetSpriteById(id);
                }
                m_dragStartPos = currentWorldPos;
                m_viewport.RefreshTexture(m_engine);
            }
        }

        // 5. MOUSE RELEASE (RECORD MOVE COMMAND TO UNDO STACK)
        if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
            if (m_isDragging) {
                auto proj = m_engine.GetCurrentProject();
                if (proj) {
                    for (const auto& startData : m_dragStartRects) {
                        auto currentSprite = proj->GetSpriteById(startData.first);
                        if (currentSprite) {
                            StudioCore::Rect finalRect = currentSprite->GetSourceRect();
                            if (startData.second.x != finalRect.x || startData.second.y != finalRect.y) {
                                m_engine.MoveSprite(startData.first, startData.second, finalRect);
                            }
                        }
                    }
                }
                m_isDragging = false;
                m_draggedSprites.clear();
                m_dragStartRects.clear();
                m_viewport.RefreshTexture(m_engine);
            }
        }

        // 6. WORKSPACE UI EVENT HANDLING
        if (m_workspace.HandleEvent(event, m_window)) {
            continue;
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
                        m_isExportMode = false;
                    }
                }
            } else {
                m_exportPreview.HandleEvent(event, m_window, m_engine);
            }
            continue;
        }

        // 7. VIEWPORT AND ANIMATION PANEL (Passes events here so Alt+Click Pivot editing works!)
        m_viewport.HandleEvent(event, m_window, m_engine);
        if (m_animationPanel) {
            m_animationPanel->HandleEvent(event, m_window, m_engine, m_viewport);
        }
    }
}

void MainApplicationWindow::Update(float deltaTime) {
    if (!m_isExportMode && !m_isWizardMode) {
        m_engine.Update(deltaTime);
        m_viewport.Update(deltaTime);

        m_autoSaveTimer += deltaTime;
        if (m_autoSaveTimer >= 300.0f) {
            if (m_engine.IsProjectActive()) {
                m_engine.SaveProject("autosave_backup.sps");
            }
            m_autoSaveTimer = 0.0f;
        }

        sf::Vector2i pixelPos = sf::Mouse::getPosition(m_window);
        sf::Vector2f worldPos = m_window.mapPixelToCoords(pixelPos);
        int selectedCount = (m_engine.IsProjectActive() && m_engine.GetCurrentProject()) ? static_cast<int>(m_engine.GetCurrentProject()->GetSprites().size()) : 0;

        m_workspace.UpdateStatusBar(
            1.0f, 
            worldPos, 
            selectedCount, 
            "None", 
            0, 
            "1920x1080"
        );
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

        m_workspace.Render(m_window);
    }
    
    m_window.display();
}