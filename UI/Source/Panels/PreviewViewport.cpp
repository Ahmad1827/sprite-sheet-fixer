#include "Panels/PreviewViewport.h"
#include "StudioEngineFacade.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/Project.h"
#include "DataModels/SpriteDefinition.h"
#include <cmath>

PreviewViewport::PreviewViewport() {
    m_view.setSize(800.f, 600.f);
    m_view.setCenter(400.f, 300.f);
}

void PreviewViewport::Initialize() {
    m_overlay.InitializeFont("Resources/font.ttf");
}

void PreviewViewport::RefreshTexture(const StudioCore::StudioEngineFacade& engine) {
    if (engine.HasTexture()) {
        auto coreTex = engine.GetCurrentTexture();
        m_gpuTexture.create(coreTex->GetWidth(), coreTex->GetHeight());
        m_gpuTexture.update(coreTex->GetPixels().data());
        m_gpuTexture.setSmooth(false); 
        m_sprite.setTexture(m_gpuTexture, true);
        m_hasValidTexture = true;
        
        m_selection.ClearSelection();
        m_selectedSpriteId.clear();
        m_hoveredSpriteId.clear();
        ResetZoom();
        CenterImage();
    }
}

void PreviewViewport::ResetZoom() {
    float ratio = 1.0f / m_currentZoom;
    m_view.zoom(ratio);
    m_currentZoom = 1.0f;
}

void PreviewViewport::ResetPan() {
    m_view.setCenter(0.0f, 0.0f); 
}

void PreviewViewport::CenterImage() {
    if (!m_hasValidTexture) return;
    sf::FloatRect bounds = m_sprite.getLocalBounds();
    m_view.setCenter(bounds.width / 2.0f, bounds.height / 2.0f);
}

void PreviewViewport::FitToImage(const sf::RenderWindow& window) {
    if (!m_hasValidTexture || window.getSize().x == 0) return;
    sf::FloatRect bounds = m_sprite.getLocalBounds();
    m_view.setCenter(bounds.width / 2.0f, bounds.height / 2.0f);
    
    float padX = static_cast<float>(window.getSize().x) * 0.9f;
    float padY = static_cast<float>(window.getSize().y) * 0.9f;
    float ratioX = bounds.width / padX;
    float ratioY = bounds.height / padY;
    
    m_currentZoom = std::max(ratioX, ratioY);
    m_view.setSize(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    m_view.zoom(m_currentZoom);
}

void PreviewViewport::ZoomToSelection() {
    if (!m_selection.HasValidSelection()) return;
    sf::FloatRect rect = m_selection.GetSelectionRect();
    if (rect.width > 0 && rect.height > 0) {
        m_view.setCenter(rect.left + rect.width / 2.0f, rect.top + rect.height / 2.0f);
        m_view.setSize(rect.width * 1.2f, rect.height * 1.2f); 
        m_currentZoom = m_view.getSize().x / 1280.f; 
    }
}

void PreviewViewport::CenterOnPoint(const sf::Vector2f& worldPos) {
    sf::Vector2f currentCameraCenter = m_view.getCenter();
    sf::Vector2f panOffset = worldPos - currentCameraCenter;
    m_view.move(panOffset);
}

sf::Vector2i PreviewViewport::GetMouseImageCoords(const sf::RenderWindow& window) const {
    sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), m_view);
    return sf::Vector2i(static_cast<int>(std::floor(worldPos.x)), static_cast<int>(std::floor(worldPos.y)));
}

sf::Color PreviewViewport::GetPixelColor(const StudioCore::StudioEngineFacade& engine, int x, int y) const {
    if (!engine.HasTexture()) return sf::Color::Transparent;
    auto tex = engine.GetCurrentTexture();
    if (x >= 0 && x < tex->GetWidth() && y >= 0 && y < tex->GetHeight()) {
        size_t byteIdx = static_cast<size_t>(y * tex->GetWidth() + x) * 4;
        const auto& p = tex->GetPixels();
        return sf::Color(p[byteIdx], p[byteIdx+1], p[byteIdx+2], p[byteIdx+3]);
    }
    return sf::Color::Transparent;
}

void PreviewViewport::SelectSpriteAt(const sf::Vector2f& worldPos, const StudioCore::StudioEngineFacade& engine) {
    if (!engine.IsProjectActive()) return;

    auto project = engine.GetCurrentProject();
    m_selectedSpriteId.clear();

    for (const auto& sprite : project->GetSprites()) {
        const auto& rect = sprite->GetSourceRect();
        sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
        if (bounds.contains(worldPos)) {
            m_selectedSpriteId = sprite->GetId();
            break;
        }
    }
}

void PreviewViewport::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine) {
    if (event.type == sf::Event::KeyPressed) {
        // Ctrl + D -> Trigger Auto Detect
        if (event.key.code == sf::Keyboard::D && event.key.control) {
            StudioCore::DetectionConfig config; // Uses defaults: AlphaThreshold, 8-Way
            engine.RunAutoDetection(config);
        }
        // Escape -> Cancel Running Detection
        else if (event.key.code == sf::Keyboard::Escape) {
            if (engine.IsDetectionRunning()) {
                engine.CancelDetection();
            }
        }
        else if (event.key.code == sf::Keyboard::G) m_showGrid = !m_showGrid;
        else if (event.key.code == sf::Keyboard::F3) m_showDebug = !m_showDebug;
        else if (event.key.code == sf::Keyboard::C) CenterImage();
        else if (event.key.code == sf::Keyboard::R) {
            if (event.key.shift) ResetPan();
            else ResetZoom();
        }
        else if (event.key.code == sf::Keyboard::F) FitToImage(window);
        else if (event.key.code == sf::Keyboard::Z) ZoomToSelection();
        else if (event.key.code == sf::Keyboard::B) m_showBoxes = !m_showBoxes;
        else if (event.key.code == sf::Keyboard::I) m_showIds = !m_showIds;
    }
    
    else if (event.type == sf::Event::MouseWheelScrolled) {
        float zoomFactor = (event.mouseWheelScroll.delta > 0) ? 0.8f : 1.25f;
        m_view.zoom(zoomFactor);
        m_currentZoom *= zoomFactor;
    } 
    
    else if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = true;
            m_lastMousePos = sf::Mouse::getPosition(window);
        } else if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i clickScreenPos(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f clickedImagePos = window.mapPixelToCoords(clickScreenPos, m_view);

            if (m_doubleClickTimer.getElapsedTime().asSeconds() < 0.3f) {
                CenterOnPoint(clickedImagePos);
                m_selection.ClearSelection();
            } else {
                SelectSpriteAt(clickedImagePos, engine);
                m_selection.BeginSelection(clickedImagePos);
            }
            m_doubleClickTimer.restart();
        }
    } 
    
    else if (event.type == sf::Event::MouseButtonReleased) {
        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = false;
        } else if (event.mouseButton.button == sf::Mouse::Left) {
            m_selection.EndSelection();
        }
    } 
    
    else if (event.type == sf::Event::MouseMoved) {
        sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos = window.mapPixelToCoords(currentMousePos, m_view);

        // Hover tracking over detected sprites
        if (engine.IsProjectActive()) {
            m_hoveredSpriteId.clear();
            auto project = engine.GetCurrentProject();
            for (const auto& sprite : project->GetSprites()) {
                const auto& rect = sprite->GetSourceRect();
                sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
                if (bounds.contains(worldPos)) {
                    m_hoveredSpriteId = sprite->GetId();
                    break;
                }
            }
        }

        m_selection.UpdateSelection(worldPos);

        if (m_isPanning) {
            sf::Vector2f delta(
                static_cast<float>(m_lastMousePos.x - currentMousePos.x),
                static_cast<float>(m_lastMousePos.y - currentMousePos.y)
            );
            delta.x *= (m_view.getSize().x / window.getSize().x);
            delta.y *= (m_view.getSize().y / window.getSize().y);
            m_view.move(delta);
            m_lastMousePos = currentMousePos;
        }
    } 
    
    else if (event.type == sf::Event::Resized) {
        sf::Vector2f center = m_view.getCenter();
        m_view.setSize(static_cast<float>(event.size.width) * m_currentZoom, 
                       static_cast<float>(event.size.height) * m_currentZoom);
        m_view.setCenter(center);
    }
}

void PreviewViewport::Update(float deltaTime) {
    m_frameTimeMs = deltaTime * 1000.0f;
    m_frameCount++;
    m_fpsTimer += deltaTime;
    if (m_fpsTimer >= 1.0f) {
        m_fps = static_cast<float>(m_frameCount) / m_fpsTimer;
        m_frameCount = 0;
        m_fpsTimer = 0.0f;
    }
}

void PreviewViewport::Render(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    // 1. WORLD SPACE
    window.setView(m_view);

    if (m_hasValidTexture) {
        window.draw(m_sprite);
        m_axes.Render(window, m_sprite.getLocalBounds(), m_currentZoom);
    }

    if (m_showGrid && m_hasValidTexture) {
        m_grid.Render(window, m_sprite.getLocalBounds());
    }

    // Render detected sprites bounding boxes
    if (engine.IsProjectActive()) {
        auto project = engine.GetCurrentProject();
        m_spriteGizmos.SetHoveredSprite(m_hoveredSpriteId);
        m_spriteGizmos.SetSelectedSprite(m_selectedSpriteId);
        m_spriteGizmos.Render(
            window, 
            project->GetSprites(), 
            m_currentZoom, 
            m_showBoxes, 
            m_showCenters, 
            m_showIds, 
            m_overlay.GetFont()
        );
    }

    m_selection.Render(window, m_currentZoom);

    // 2. HUD SPACE
    window.setView(window.getDefaultView());

    // Detection Job Progress Bar
    StudioUI::JobProgressInfo jobInfo;
    jobInfo.isRunning = engine.IsDetectionRunning();
    jobInfo.progress = engine.GetDetectionProgress();
    m_overlay.RenderProgress(window, jobInfo);

    if (m_showDebug) {
        StudioUI::DebugInfo debug;
        debug.fps = m_fps;
        debug.frameTimeMs = m_frameTimeMs;
        debug.zoomLevel = m_currentZoom;
        debug.panOffset = m_view.getCenter();
        debug.viewportSize = m_view.getSize();
        debug.projectLoaded = engine.IsProjectActive();
        debug.textureLoaded = engine.HasTexture();
        
        if (engine.HasTexture()) {
            debug.imageWidth = engine.GetCurrentTexture()->GetWidth();
            debug.imageHeight = engine.GetCurrentTexture()->GetHeight();
        }
        if (m_selection.HasValidSelection()) {
            auto rect = m_selection.GetSelectionRect();
            debug.selectionWidth = rect.width;
            debug.selectionHeight = rect.height;
        }
        m_overlay.RenderDebug(window, debug);
    }

    // Mouse Inspector
    StudioUI::InspectorInfo inspector;
    inspector.mouseWindow = sf::Mouse::getPosition(window);
    inspector.mouseImage = GetMouseImageCoords(window);
    inspector.currentZoom = m_currentZoom;
    inspector.panOffset = m_view.getCenter();
    
    if (engine.HasTexture()) {
        auto tex = engine.GetCurrentTexture();
        inspector.imageWidth = tex->GetWidth();
        inspector.imageHeight = tex->GetHeight();
        inspector.isHoveringImage = (inspector.mouseImage.x >= 0 && inspector.mouseImage.x < tex->GetWidth() &&
                                     inspector.mouseImage.y >= 0 && inspector.mouseImage.y < tex->GetHeight());
        
        if (inspector.isHoveringImage) {
            inspector.pixelColor = GetPixelColor(engine, inspector.mouseImage.x, inspector.mouseImage.y);
        }
    }
    m_overlay.RenderInspector(window, inspector);

    // Selected Sprite Inspector Overlay
    if (engine.IsProjectActive() && !m_selectedSpriteId.empty()) {
        auto project = engine.GetCurrentProject();
        for (const auto& sprite : project->GetSprites()) {
            if (sprite->GetId() == m_selectedSpriteId) {
                StudioUI::SpriteInspectorInfo spriteInfo;
                spriteInfo.isActive = true;
                spriteInfo.id = sprite->GetId();
                const auto& r = sprite->GetSourceRect();
                spriteInfo.x = r.x; spriteInfo.y = r.y;
                spriteInfo.w = r.width; spriteInfo.h = r.height;
                spriteInfo.pixelCount = sprite->GetPixelCount();
                const auto& c = sprite->GetCenter();
                spriteInfo.cx = c.x; spriteInfo.cy = c.y;
                m_overlay.RenderSpriteInspector(window, spriteInfo);
                break;
            }
        }
    }
}