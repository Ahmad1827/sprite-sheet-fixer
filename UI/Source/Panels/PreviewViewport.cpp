#include "Panels/PreviewViewport.h"
#include "StudioEngineFacade.h"
#include "DataModels/SourceTexture.h"

PreviewViewport::PreviewViewport() {
    m_view.setSize(800.f, 600.f);
    m_view.setCenter(400.f, 300.f);
}

void PreviewViewport::RefreshTexture(const StudioCore::StudioEngineFacade& engine) {
    if (engine.HasTexture()) {
        auto coreTex = engine.GetCurrentTexture();
        m_gpuTexture.create(coreTex->GetWidth(), coreTex->GetHeight());
        m_gpuTexture.update(coreTex->GetPixels().data());
        m_gpuTexture.setSmooth(false); 
        m_sprite.setTexture(m_gpuTexture, true);
        m_hasValidTexture = true;
        ResetView();
    }
}

void PreviewViewport::ResetView() {
    if (!m_hasValidTexture) return;
    
    sf::FloatRect bounds = m_sprite.getLocalBounds();
    m_view.setCenter(bounds.width / 2.0f, bounds.height / 2.0f);
}

void PreviewViewport::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (event.type == sf::Event::MouseWheelScrolled) {
        if (event.mouseWheelScroll.delta > 0) {
            m_view.zoom(0.8f);
        } else {
            m_view.zoom(1.25f);
        }
    } else if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = true;
            m_lastMousePos = sf::Mouse::getPosition(window);
        }
    } else if (event.type == sf::Event::MouseButtonReleased) {
        if (event.mouseButton.button == sf::Mouse::Middle) {
            m_isPanning = false;
        }
    } else if (event.type == sf::Event::MouseMoved) {
        if (m_isPanning) {
            sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
            sf::Vector2f delta(
                static_cast<float>(m_lastMousePos.x - currentMousePos.x),
                static_cast<float>(m_lastMousePos.y - currentMousePos.y)
            );

            delta.x *= (m_view.getSize().x / window.getSize().x);
            delta.y *= (m_view.getSize().y / window.getSize().y);

            m_view.move(delta);
            m_lastMousePos = currentMousePos;
        }
    } else if (event.type == sf::Event::Resized) {
        m_view.setSize(static_cast<float>(event.size.width), static_cast<float>(event.size.height));
    }
}

void PreviewViewport::Update(float deltaTime) {
    
}

void PreviewViewport::Render(sf::RenderWindow& window) {
    if (m_hasValidTexture) {
        window.setView(m_view);
        window.draw(m_sprite);
        window.setView(window.getDefaultView());
    }
}