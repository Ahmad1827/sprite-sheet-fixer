#pragma once
#include <SFML/Graphics.hpp>
#include <string>

namespace StudioCore {
    class StudioEngineFacade;
}

namespace StudioUI {

class ExportPreviewPanel {
public:
    ExportPreviewPanel();
    bool InitializeFont(const std::string& customPath);
    void Activate(const StudioCore::StudioEngineFacade& engine);
    void Deactivate() { m_isActive = false; }
    bool HandleEvent(const sf::Event& event, const sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    void Render(sf::RenderWindow& window);

    bool IsActive() const { return m_isActive; }
    const std::string& GetLastExportPath() const { return m_lastExportPath; }

private:
    void RefreshPreview(const StudioCore::StudioEngineFacade& engine);
    void Save(const StudioCore::StudioEngineFacade& engine);
    // Bottom bar buttons, laid out from the window size.
    sf::FloatRect PackedRect() const;
    sf::FloatRect OriginalRect() const;
    sf::FloatRect SaveRect() const;
    sf::FloatRect CancelRect() const;
    sf::Vector2f m_windowSize{1280.0f, 720.0f};
    sf::Vector2f m_mousePos{-1.0f, -1.0f};
    bool m_needsFit{true};

    sf::Font m_font;
    bool m_hasFont{false};
    
    sf::Texture m_exportTexture;
    sf::Sprite m_exportSprite;
    sf::View m_view;

    bool m_isPanning{false};
    sf::Vector2i m_lastMousePos;
    float m_currentZoom{1.0f};
    bool m_isActive{false};
    bool m_keepOriginalResolution{false};
    std::string m_lastExportPath;
};

}