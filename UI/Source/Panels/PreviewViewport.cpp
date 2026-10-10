#include "Panels/PreviewViewport.h"
#include "StudioEngineFacade.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/Project.h"
#include "DataModels/SpriteDefinition.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include "Theme.h"

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
        
        bool isNewImage = (!m_hasValidTexture || 
                           m_gpuTexture.getSize().x != coreTex->GetWidth() || 
                           m_gpuTexture.getSize().y != coreTex->GetHeight());

        m_gpuTexture.create(coreTex->GetWidth(), coreTex->GetHeight());
        m_gpuTexture.update(coreTex->GetPixels().data());
        m_gpuTexture.setSmooth(false); 
        m_sprite.setTexture(m_gpuTexture, true);
        m_hasValidTexture = true;
        
        if (isNewImage) {
            m_selection.ClearSelection();
            m_selectedSpriteIds.clear();
            m_hoveredSpriteId.clear();
            FitToCanvas();
        }
    }
}

void PreviewViewport::SetCanvasBounds(const sf::FloatRect& canvas) {
    const bool resized = canvas.width != m_canvas.width || canvas.height != m_canvas.height;
    m_canvas = canvas;
    if (resized && m_isFitted && m_hasValidTexture) {
        FitToCanvas();
    } else {
        ApplyView();
    }
}

void PreviewViewport::ApplyView() {
    sf::Vector2f center = m_view.getCenter();
    m_view.setSize(m_canvas.width * m_currentZoom, m_canvas.height * m_currentZoom);
    m_view.setCenter(center);
    if (m_bounds.width > 0.0f && m_bounds.height > 0.0f) {
        m_view.setViewport(sf::FloatRect(
            (m_canvas.left - m_bounds.left) / m_bounds.width,
            (m_canvas.top - m_bounds.top) / m_bounds.height,
            m_canvas.width / m_bounds.width,
            m_canvas.height / m_bounds.height));
    }
}

void PreviewViewport::FitToCanvas() {
    if (!m_hasValidTexture || m_canvas.width <= 0.0f || m_canvas.height <= 0.0f) return;
    sf::FloatRect bounds = m_sprite.getLocalBounds();
    m_view.setCenter(bounds.width / 2.0f, bounds.height / 2.0f);
    m_currentZoom = std::max(bounds.width / (m_canvas.width * 0.92f), bounds.height / (m_canvas.height * 0.92f));
    m_isFitted = true;
    ApplyView();
}

void PreviewViewport::ResetZoom() {
    m_isFitted = false;
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
    (void)window;
    FitToCanvas();
}

void PreviewViewport::ZoomToSelection() {
    if (!m_selection.HasValidSelection()) return;
    sf::FloatRect rect = m_selection.GetSelectionRect();
    if (rect.width > 0 && rect.height > 0) {
        m_view.setCenter(rect.left + rect.width / 2.0f, rect.top + rect.height / 2.0f);
        float baseWidth = m_canvas.width > 0 ? m_canvas.width : 1280.f;
        m_currentZoom = (rect.width * 1.2f) / baseWidth;
        m_isFitted = false;
    }
}

void PreviewViewport::CenterOnPoint(const sf::Vector2f& worldPos) {
    sf::Vector2f currentCameraCenter = m_view.getCenter();
    sf::Vector2f panOffset = worldPos - currentCameraCenter;
    m_view.move(panOffset);
    m_isFitted = false;
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

void PreviewViewport::HandleSpriteSelection(const sf::Vector2f& worldPos, const StudioCore::StudioEngineFacade& engine, bool shift, bool ctrl) {
    if (!engine.IsProjectActive()) return;

    auto project = engine.GetCurrentProject();
    std::string clickedId = "";

    for (const auto& sprite : project->GetSprites()) {
        const auto& rect = sprite->GetSourceRect();
        sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
        if (bounds.contains(worldPos)) {
            clickedId = sprite->GetId();
            break;
        }
    }

    if (clickedId.empty()) {
        if (!ctrl && !shift) m_selectedSpriteIds.clear();
        return;
    }

    auto it = std::find(m_selectedSpriteIds.begin(), m_selectedSpriteIds.end(), clickedId);

    if (ctrl) {
        if (it != m_selectedSpriteIds.end()) m_selectedSpriteIds.erase(it);
        else m_selectedSpriteIds.push_back(clickedId);
    } else if (shift) {
        if (it == m_selectedSpriteIds.end()) m_selectedSpriteIds.push_back(clickedId);
    } else {
        m_selectedSpriteIds.clear();
        m_selectedSpriteIds.push_back(clickedId);
    }
}

void PreviewViewport::TriggerNumericEdit(StudioCore::StudioEngineFacade& engine) {
    if (m_selectedSpriteIds.empty()) return;
    float px, py;
    if (std::cin >> px >> py) {
        engine.EditPivot(m_selectedSpriteIds, {px, py});
    }
}

void PreviewViewport::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine) {
    ApplyView();

    // Clicks and scrolling that start outside the canvas belong to a bar or panel.
    if (event.type == sf::Event::MouseButtonPressed &&
        !m_canvas.contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
        return;
    }
    if (event.type == sf::Event::MouseWheelScrolled &&
        !m_canvas.contains(static_cast<float>(event.mouseWheelScroll.x), static_cast<float>(event.mouseWheelScroll.y))) {
        return;
    }

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Z && event.key.control) {
            if (event.key.shift) engine.Redo();
            else engine.Undo();
        }
        else if (event.key.code == sf::Keyboard::Y && event.key.control) {
            engine.Redo();
        }
        else if (event.key.code == sf::Keyboard::A && event.key.control) {
            m_selectedSpriteIds.clear();
            if (engine.IsProjectActive()) {
                for (const auto& s : engine.GetCurrentProject()->GetSprites()) {
                    m_selectedSpriteIds.push_back(s->GetId());
                }
            }
        }
        else if (event.key.code == sf::Keyboard::D && event.key.control) {
            StudioCore::DetectionConfig config;
            engine.RunAutoDetection(config);
        }
        else if (event.key.code == sf::Keyboard::Escape) {
            if (engine.IsDetectionRunning()) engine.CancelDetection();
            m_selectedSpriteIds.clear();
            m_selection.ClearSelection();
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
        else if (event.key.code == sf::Keyboard::P) m_showPivots = !m_showPivots;
        else if (event.key.code == sf::Keyboard::L) m_showBaselines = !m_showBaselines;
        else if (event.key.code == sf::Keyboard::N) TriggerNumericEdit(engine);
    } 
    else if (event.type == sf::Event::MouseWheelScrolled) {
        // Zoom around the cursor so the point under it stays put.
        sf::Vector2i cursor(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
        sf::Vector2f before = window.mapPixelToCoords(cursor, m_view);
        float zoomFactor = (event.mouseWheelScroll.delta > 0) ? 0.8f : 1.25f;
        m_currentZoom = std::clamp(m_currentZoom * zoomFactor, 0.01f, 40.0f);
        m_isFitted = false;
        ApplyView();
        sf::Vector2f after = window.mapPixelToCoords(cursor, m_view);
        m_view.move(before - after);
    }
    else if (event.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i clickScreenPos(event.mouseButton.x, event.mouseButton.y);

        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = true;
            m_lastMousePos = sf::Mouse::getPosition(window);
        } else if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2f clickedImagePos = window.mapPixelToCoords(clickScreenPos, m_view);

            bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
            bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
            bool alt = sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt);

            if (m_doubleClickTimer.getElapsedTime().asSeconds() < 0.3f) {
                if (!m_selectedSpriteIds.empty()) {
                    engine.EditPivot(m_selectedSpriteIds, {0.5f, 1.0f});
                    engine.EditBaseline(m_selectedSpriteIds, 0.0f);
                } else {
                    CenterOnPoint(clickedImagePos);
                }
                m_selection.ClearSelection();
            } else {
                if (ctrl && alt && !m_selectedSpriteIds.empty()) {
                    m_isDraggingBaseline = true;
                    m_isDraggingPivot = false;
                } else if (alt && !m_selectedSpriteIds.empty()) {
                    m_isDraggingPivot = true;
                    m_isDraggingBaseline = false;
                } else {
                    HandleSpriteSelection(clickedImagePos, engine, shift, ctrl);
                    if (m_selectedSpriteIds.empty() || shift || ctrl) { 
                        m_selection.BeginSelection(clickedImagePos);
                    }
                }
            }
            m_doubleClickTimer.restart();
        }
    }
    else if (event.type == sf::Event::MouseButtonReleased) {
        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = false;
        } else if (event.mouseButton.button == sf::Mouse::Left) {
            if (m_selection.HasValidSelection() && engine.IsProjectActive()) {
                sf::FloatRect marquee = m_selection.GetSelectionRect();
                auto project = engine.GetCurrentProject();
                
                bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
                bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
                if (!shift && !ctrl) m_selectedSpriteIds.clear();

                for (const auto& sprite : project->GetSprites()) {
                    const auto& rect = sprite->GetSourceRect();
                    sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
                    
                    if (marquee.intersects(bounds)) {
                        if (std::find(m_selectedSpriteIds.begin(), m_selectedSpriteIds.end(), sprite->GetId()) == m_selectedSpriteIds.end()) {
                            m_selectedSpriteIds.push_back(sprite->GetId());
                        }
                    }
                }
            }
            m_selection.EndSelection();
            m_selection.ClearSelection();
            m_isDraggingPivot = false;
            m_isDraggingBaseline = false;
        }
    } 
    else if (event.type == sf::Event::MouseMoved) {
        sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos = window.mapPixelToCoords(currentMousePos, m_view);

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

            if (m_isDraggingPivot && !m_selectedSpriteIds.empty()) {
                auto primary = project->GetSpriteById(m_selectedSpriteIds[0]);
                if (primary) {
                    const auto& rect = primary->GetSourceRect();
                    float nx = std::clamp((worldPos.x - rect.x) / rect.width, 0.0f, 1.0f);
                    float ny = std::clamp((worldPos.y - rect.y) / rect.height, 0.0f, 1.0f);
                    
                    float dx = nx - primary->GetPivot().x;
                    float dy = ny - primary->GetPivot().y;

                    for (const auto& id : m_selectedSpriteIds) {
                        auto s = project->GetSpriteById(id);
                        if (s) {
                            engine.EditPivot({id}, {s->GetPivot().x + dx, s->GetPivot().y + dy});
                        }
                    }
                }
            }

            if (m_isDraggingBaseline && !m_selectedSpriteIds.empty()) {
                auto primary = project->GetSpriteById(m_selectedSpriteIds[0]);
                if (primary) {
                    const auto& rect = primary->GetSourceRect();
                    float offset = (rect.y + rect.height) - worldPos.y;
                    engine.EditBaseline(m_selectedSpriteIds, offset);
                }
            }
        }

        m_selection.UpdateSelection(worldPos);

        if (m_isPanning) {
            sf::Vector2f delta(
                static_cast<float>(m_lastMousePos.x - currentMousePos.x),
                static_cast<float>(m_lastMousePos.y - currentMousePos.y)
            );
            delta.x *= m_currentZoom;
            delta.y *= m_currentZoom;
            m_view.move(delta);
            m_isFitted = false;
            m_lastMousePos = currentMousePos;
        }
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
    sf::View uiView(m_bounds);
    window.setView(uiView);
    sf::RectangleShape canvasBackground({ m_canvas.width, m_canvas.height });
    canvasBackground.setPosition(m_canvas.left, m_canvas.top);
    canvasBackground.setFillColor(StudioUI::Theme::ViewportBackground);
    window.draw(canvasBackground);

    ApplyView();
    window.setView(m_view);

    if (m_hasValidTexture) {
        // The image area is a touch lighter than the canvas so its edges, and
        // what is transparent inside it, are easy to see.
        sf::FloatRect imageBounds = m_sprite.getLocalBounds();
        sf::RectangleShape imageBackground({ imageBounds.width, imageBounds.height });
        imageBackground.setFillColor(sf::Color(27, 29, 35));
        window.draw(imageBackground);

        if (m_showGrid) {
            m_grid.Render(window, m_sprite.getLocalBounds());
        }

        window.draw(m_sprite);

        sf::FloatRect bounds = m_sprite.getLocalBounds();
        sf::RectangleShape canvasBorder(sf::Vector2f(bounds.width, bounds.height));
        canvasBorder.setFillColor(sf::Color::Transparent);
        canvasBorder.setOutlineThickness(1.0f / m_currentZoom);
        canvasBorder.setOutlineColor(StudioUI::Theme::BorderColor);
        window.draw(canvasBorder);

        // Rulers and axes are guides, shown together with the grid (G).
        if (m_showGrid) m_axes.Render(window, m_sprite.getLocalBounds(), m_currentZoom);
    }

    if (engine.IsProjectActive()) {
        auto project = engine.GetCurrentProject();
        m_spriteGizmos.SetHoveredSprite(m_hoveredSpriteId);
        m_spriteGizmos.SetSelectedSprites(m_selectedSpriteIds);
        m_spriteGizmos.Render(window, project->GetSprites(), m_currentZoom, m_showBoxes, m_showCenters, m_showPivots, m_showBaselines, m_showIds, m_overlay.GetFont());
    }

    m_selection.Render(window, m_currentZoom);

    window.setView(uiView);

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
        m_overlay.RenderDebug(window, debug);
    }

    if (!m_hasValidTexture) {
        window.setView(uiView);
        m_overlay.RenderEmptyState(window, m_canvas);
    }
}