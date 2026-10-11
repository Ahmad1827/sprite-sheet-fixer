#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>

namespace StudioCore {
    class StudioEngineFacade;
}

class PreviewViewport;

namespace StudioUI {

class AnimationPanel {
public:
    AnimationPanel();
    bool InitializeFont(const std::string& customPath);
    void HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, PreviewViewport& viewport);
    void Render(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    void SetBounds(const sf::FloatRect& bounds);
    // Space kept free at the bottom of the window (the status bar).
    void SetBottomInset(float inset) { m_bottomInset = inset; }

private:
    sf::Font m_font;
    bool m_hasFont{false};
    
    sf::FloatRect m_listArea;
    sf::FloatRect m_timelineArea;
    sf::FloatRect m_previewArea;
    
    sf::FloatRect m_playBtnArea;
    sf::FloatRect m_pauseBtnArea;
    sf::FloatRect m_stopBtnArea;
    sf::FloatRect m_fpsMinusBtnArea;
    sf::FloatRect m_fpsPlusBtnArea;

    sf::RectangleShape m_bgShape;
    
    std::string m_selectedAnimation;
    int m_draggedFrameIndex{-1};
    int m_dropTargetIndex{-1};
    float m_listScroll{0.0f};   // how far the animation list is scrolled, in pixels

    void UpdateLayout(const StudioCore::StudioEngineFacade& engine);
    void RenderList(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    void RenderTimeline(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    void RenderPreview(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine);
    
    sf::FloatRect m_bounds{0.0f, 0.0f, 1280.0f, 720.0f};
    float m_bottomInset{0.0f};
};

}