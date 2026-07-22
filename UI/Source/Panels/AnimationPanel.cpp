#include "Panels/AnimationPanel.h"
#include "Panels/PreviewViewport.h"
#include "StudioEngineFacade.h"
#include "DataModels/Project.h"
#include "DataModels/AnimationGroup.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/SourceTexture.h"
#include "Systems/PlaybackEngine.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "Theme.h"

namespace StudioUI {

AnimationPanel::AnimationPanel() {
    m_bgShape.setFillColor(sf::Color(40, 40, 40, 255));
    m_bgShape.setOutlineColor(sf::Color(100, 100, 100));
    m_bgShape.setOutlineThickness(-1.0f);
}

bool AnimationPanel::InitializeFont(const std::string& customPath) {
    std::vector<std::string> fontPaths = {
        customPath,
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };
    for (const auto& path : fontPaths) {
        if (m_font.loadFromFile(path)) {
            m_hasFont = true;
            return true;
        }
    }
    std::cerr << "[AnimationPanel] Failed to load font!" << std::endl;
    return false;
}

void AnimationPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, PreviewViewport& viewport) {
    if (!engine.IsProjectActive()) return;

    sf::Vector2u winSize = window.getSize();
    m_listArea = sf::FloatRect(winSize.x - 300.f, 0.f, 300.f, winSize.y / 2.f);
    m_previewArea = sf::FloatRect(winSize.x - 300.f, winSize.y / 2.f, 300.f, winSize.y / 2.f);
    m_timelineArea = sf::FloatRect(0.f, winSize.y - 200.f, winSize.x - 300.f, 200.f);

    const auto& selectedSprites = viewport.GetSelectedSpriteIds();

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::N && event.key.shift) {
            engine.CreateAnimation("New Animation");
        } else if (event.key.code == sf::Keyboard::Space) {
            if (engine.GetPlaybackEngine().IsPlaying()) {
                engine.GetPlaybackEngine().Pause();
            } else {
                engine.GetPlaybackEngine().Play();
            }
        } 
        else if (event.key.code == sf::Keyboard::A) {
            engine.ToggleAutoAlign();
        }
        else if ((event.key.code == sf::Keyboard::Delete || event.key.code == sf::Keyboard::BackSpace) && !m_selectedAnimation.empty()) {
            engine.ModifyAnimationFrames(m_selectedAnimation, {});
        }
    } 
    else if (event.type == sf::Event::MouseButtonPressed) {
        sf::Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);

        if (event.mouseButton.button == sf::Mouse::Left) {
            // --- PLAYBACK BUTTON CLICKS ---
            if (m_playBtnArea.contains(mousePos)) {
                engine.GetPlaybackEngine().Play();
                return;
            } else if (m_pauseBtnArea.contains(mousePos)) {
                engine.GetPlaybackEngine().Pause();
                return;
            } else if (m_stopBtnArea.contains(mousePos)) {
                engine.GetPlaybackEngine().Stop();
                return;
            } 
            // --- FPS CONTROL CLICKS ---
            else if (!m_selectedAnimation.empty()) {
                auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
                if (anim) {
                    if (m_fpsMinusBtnArea.contains(mousePos)) {
                        float newFps = std::max(1.0f, anim->GetFPS() - 1.0f);
                        engine.EditAnimationSettings(m_selectedAnimation, anim->GetName(), newFps, anim->IsLooping());
                        return;
                    } else if (m_fpsPlusBtnArea.contains(mousePos)) {
                        float newFps = std::min(60.0f, anim->GetFPS() + 1.0f);
                        engine.EditAnimationSettings(m_selectedAnimation, anim->GetName(), newFps, anim->IsLooping());
                        return;
                    }
                }
            }

            // --- ANIMATION LIST SELECTION ---
            if (m_listArea.contains(mousePos)) {
                float yOffset = 40.f;
                auto project = engine.GetCurrentProject();
                for (const auto& anim : project->GetAnimationGroups()) {
                    sf::FloatRect itemRect(m_listArea.left + 10.f, m_listArea.top + yOffset, 280.f, 25.f);
                    if (itemRect.contains(mousePos)) {
                        m_selectedAnimation = anim->GetId();
                        engine.GetPlaybackEngine().SetActiveAnimation(m_selectedAnimation);
                        break;
                    }
                    yOffset += 30.f;
                }
            } 
            // --- TIMELINE APPEND / DRAG ---
            else if (m_timelineArea.contains(mousePos) && !m_selectedAnimation.empty()) {
                auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
                if (anim) {
                    if (!selectedSprites.empty()) {
                        auto frames = anim->GetFrames();
                        frames.insert(frames.end(), selectedSprites.begin(), selectedSprites.end());
                        engine.ModifyAnimationFrames(m_selectedAnimation, frames);
                        viewport.ClearSelection();
                    } else {
                        float xOffset = 10.f;
                        int index = 0;
                        for (const auto& frame : anim->GetFrames()) {
                            sf::FloatRect frameRect(m_timelineArea.left + xOffset, m_timelineArea.top + 30.f, 60.f, 60.f);
                            if (frameRect.contains(mousePos)) {
                                m_draggedFrameIndex = index;
                                m_dropTargetIndex = index;
                                break;
                            }
                            xOffset += 70.f;
                            index++;
                        }
                    }
                }
            }
        } 
        // Right Click on Frame -> Delete Frame
        else if (event.mouseButton.button == sf::Mouse::Right && m_timelineArea.contains(mousePos) && !m_selectedAnimation.empty()) {
            auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
            if (anim) {
                float xOffset = 10.f;
                int index = 0;
                auto frames = anim->GetFrames();
                for (auto it = frames.begin(); it != frames.end(); ++it) {
                    sf::FloatRect frameRect(m_timelineArea.left + xOffset, m_timelineArea.top + 30.f, 60.f, 60.f);
                    if (frameRect.contains(mousePos)) {
                        frames.erase(it);
                        engine.ModifyAnimationFrames(m_selectedAnimation, frames);
                        break;
                    }
                    xOffset += 70.f;
                    index++;
                }
            }
        }
    } 
    else if (event.type == sf::Event::MouseMoved && m_draggedFrameIndex != -1) {
        sf::Vector2f mousePos(event.mouseMove.x, event.mouseMove.y);
        if (m_timelineArea.contains(mousePos)) {
            float relX = mousePos.x - m_timelineArea.left - 10.f;
            m_dropTargetIndex = std::max(0, static_cast<int>(std::round(relX / 70.f)));
        }
    }
    else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
        if (m_draggedFrameIndex != -1 && !m_selectedAnimation.empty()) {
            auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
            if (anim) {
                auto frames = anim->GetFrames();
                int maxIdx = static_cast<int>(frames.size()) - 1;
                int target = std::max(0, std::min(m_dropTargetIndex, maxIdx));

                if (target != m_draggedFrameIndex && target >= 0 && target <= maxIdx) {
                    std::string id = frames[m_draggedFrameIndex];
                    frames.erase(frames.begin() + m_draggedFrameIndex);
                    frames.insert(frames.begin() + target, id);
                    engine.ModifyAnimationFrames(m_selectedAnimation, frames);
                }
            }
        }
        m_draggedFrameIndex = -1;
        m_dropTargetIndex = -1;
    }
}

void AnimationPanel::Render(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    sf::Vector2u winSize = window.getSize();
    float panelHeight = 160.0f;
    float posY = m_bounds.top + m_bounds.height - StudioUI::Theme::StatusBarHeight - panelHeight;
    sf::RectangleShape bg(sf::Vector2f(m_bounds.width, panelHeight));
    bg.setPosition(0.0f, posY);
    bg.setFillColor(StudioUI::Theme::PanelBackground);
    bg.setOutlineThickness(StudioUI::Theme::BorderThickness);
    bg.setOutlineColor(StudioUI::Theme::BorderColor);
    window.draw(bg);

    // 2. Header Bar
    sf::RectangleShape header(sf::Vector2f(static_cast<float>(winSize.x), StudioUI::Theme::HeaderHeight));
    header.setPosition(0.0f, posY);
    header.setFillColor(StudioUI::Theme::InspectorBackground);
    window.draw(header);

    sf::Text headerText("ANIMATION TIMELINE", m_font, 11);
    headerText.setFillColor(StudioUI::Theme::TextSecondary);
    headerText.setPosition(12.0f, posY + 6.0f);
    window.draw(headerText);

    // 3. Left Controls Panel Divider
    float leftPanelWidth = 250.0f;
    sf::RectangleShape leftBorder(sf::Vector2f(StudioUI::Theme::BorderThickness, panelHeight - StudioUI::Theme::HeaderHeight));
    leftBorder.setPosition(leftPanelWidth, posY + StudioUI::Theme::HeaderHeight);
    leftBorder.setFillColor(StudioUI::Theme::BorderColor);
    window.draw(leftBorder);

    // Playback Information
    sf::Text controlsText("▶ Play   ⏸ Pause   ⏹ Stop\n\nFPS: 12   Loop: On\nFrame: 3 / 12", m_font, 12);
    controlsText.setFillColor(StudioUI::Theme::TextPrimary);
    controlsText.setPosition(20.0f, posY + StudioUI::Theme::HeaderHeight + 20.0f);
    window.draw(controlsText);

    // 4. Structured Frame Tracks
    float timelineStartX = leftPanelWidth + 20.0f;
    float timelineY = posY + StudioUI::Theme::HeaderHeight + 30.0f;
    float frameWidth = 32.0f;
    float frameHeight = 40.0f;

    for (int i = 0; i < 20; ++i) {
        float fx = timelineStartX + (i * frameWidth);
        
        sf::RectangleShape frameBox(sf::Vector2f(frameWidth - 2.0f, frameHeight));
        frameBox.setPosition(fx, timelineY);
        frameBox.setFillColor(i % 2 == 0 ? sf::Color(45, 48, 54) : sf::Color(40, 42, 48));
        
        if (i == 3) {
            frameBox.setOutlineThickness(1.5f);
            frameBox.setOutlineColor(StudioUI::Theme::AccentColor);
            frameBox.setFillColor(StudioUI::Theme::HoverColor);
        }
        window.draw(frameBox);

        if (i % 2 == 0) {
            sf::Text tickText(std::to_string(i), m_font, 10);
            tickText.setFillColor(StudioUI::Theme::TextMuted);
            tickText.setPosition(fx + 2.0f, timelineY - 18.0f);
            window.draw(tickText);
        }
    }

    // 5. Playhead
    float playheadX = timelineStartX + (3 * frameWidth) + (frameWidth / 2.0f);
    sf::RectangleShape playheadLine(sf::Vector2f(2.0f, frameHeight + 25.0f));
    playheadLine.setPosition(playheadX, timelineY - 15.0f);
    playheadLine.setFillColor(sf::Color(235, 87, 87));
    window.draw(playheadLine);
}

void AnimationPanel::RenderList(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    m_bgShape.setPosition(m_listArea.left, m_listArea.top);
    m_bgShape.setSize(sf::Vector2f(m_listArea.width, m_listArea.height));
    window.draw(m_bgShape);

    sf::Text title("Animations (Shift+N to Create)", m_font, 16);
    title.setPosition(m_listArea.left + 10.f, m_listArea.top + 10.f);
    window.draw(title);

    float yOffset = 40.f;
    auto project = engine.GetCurrentProject();
    for (const auto& anim : project->GetAnimationGroups()) {
        sf::RectangleShape item(sf::Vector2f(280.f, 25.f));
        item.setPosition(m_listArea.left + 10.f, m_listArea.top + yOffset);
        item.setFillColor(anim->GetId() == m_selectedAnimation ? sf::Color(80, 80, 150) : sf::Color(60, 60, 60));
        window.draw(item);

        sf::Text name(anim->GetName(), m_font, 14);
        name.setPosition(item.getPosition().x + 5.f, item.getPosition().y + 3.f);
        window.draw(name);
        
        yOffset += 30.f;
    }
}

void AnimationPanel::RenderTimeline(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    m_bgShape.setPosition(m_timelineArea.left, m_timelineArea.top);
    m_bgShape.setSize(sf::Vector2f(m_timelineArea.width, m_timelineArea.height));
    window.draw(m_bgShape);

    sf::Text title("Timeline (Right-Click frame to delete, Del/Backspace to clear all)", m_font, 14);
    title.setPosition(m_timelineArea.left + 10.f, m_timelineArea.top + 5.f);
    window.draw(title);

    if (m_selectedAnimation.empty()) return;

    auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
    if (!anim) return;

    float xOffset = 10.f;
    int index = 0;
    int currentFrame = engine.GetPlaybackEngine().GetCurrentFrameIndex();

    for (const auto& frameId : anim->GetFrames()) {
        sf::RectangleShape frameBox(sf::Vector2f(60.f, 60.f));
        frameBox.setPosition(m_timelineArea.left + xOffset, m_timelineArea.top + 30.f);
        frameBox.setFillColor(index == currentFrame ? sf::Color(150, 150, 80) : sf::Color(60, 60, 60));
        
        if (index == m_draggedFrameIndex) {
            frameBox.setOutlineColor(sf::Color::Yellow);
            frameBox.setOutlineThickness(3.f);
        } else {
            frameBox.setOutlineColor(sf::Color(100, 100, 100));
            frameBox.setOutlineThickness(1.f);
        }
        window.draw(frameBox);
        
        sf::Text label(std::to_string(index), m_font, 12);
        label.setPosition(frameBox.getPosition().x + 4.f, frameBox.getPosition().y + 2.f);
        window.draw(label);

        if (m_draggedFrameIndex != -1 && index == m_dropTargetIndex) {
            sf::RectangleShape dropMarker(sf::Vector2f(4.f, 64.f));
            dropMarker.setPosition(m_timelineArea.left + xOffset - 4.f, m_timelineArea.top + 28.f);
            dropMarker.setFillColor(sf::Color::Cyan);
            window.draw(dropMarker);
        }

        xOffset += 70.f;
        index++;
    }
}

void AnimationPanel::RenderPreview(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    m_bgShape.setPosition(m_previewArea.left, m_previewArea.top);
    m_bgShape.setSize(sf::Vector2f(m_previewArea.width, m_previewArea.height));
    window.draw(m_bgShape);

    bool autoAlign = engine.IsAutoAlignEnabled();
    std::string titleStr = "Preview [Auto Align: " + std::string(autoAlign ? "ON" : "OFF") + "] (Key: A)";
    sf::Text title(titleStr, m_font, 14);
    title.setPosition(m_previewArea.left + 10.f, m_previewArea.top + 10.f);
    title.setFillColor(autoAlign ? sf::Color::Cyan : sf::Color::White);
    window.draw(title);

    // --- PLAYBACK CONTROLS (PLAY, PAUSE, STOP) ---
    float btnY = m_previewArea.top + 35.f;
    
    // Play Button
    m_playBtnArea = sf::FloatRect(m_previewArea.left + 10.f, btnY, 50.f, 22.f);
    sf::RectangleShape playBox(sf::Vector2f(m_playBtnArea.width, m_playBtnArea.height));
    playBox.setPosition(m_playBtnArea.left, m_playBtnArea.top);
    playBox.setFillColor(engine.GetPlaybackEngine().IsPlaying() ? sf::Color(0, 150, 0) : sf::Color(60, 60, 60));
    window.draw(playBox);
    sf::Text playTxt("PLAY", m_font, 11);
    playTxt.setPosition(m_playBtnArea.left + 10.f, m_playBtnArea.top + 3.f);
    window.draw(playTxt);

    // Pause Button
    m_pauseBtnArea = sf::FloatRect(m_previewArea.left + 65.f, btnY, 50.f, 22.f);
    sf::RectangleShape pauseBox(sf::Vector2f(m_pauseBtnArea.width, m_pauseBtnArea.height));
    pauseBox.setPosition(m_pauseBtnArea.left, m_pauseBtnArea.top);
    pauseBox.setFillColor(!engine.GetPlaybackEngine().IsPlaying() ? sf::Color(150, 150, 0) : sf::Color(60, 60, 60));
    window.draw(pauseBox);
    sf::Text pauseTxt("PAUSE", m_font, 11);
    pauseTxt.setPosition(m_pauseBtnArea.left + 6.f, m_pauseBtnArea.top + 3.f);
    window.draw(pauseTxt);

    // Stop Button
    m_stopBtnArea = sf::FloatRect(m_previewArea.left + 120.f, btnY, 50.f, 22.f);
    sf::RectangleShape stopBox(sf::Vector2f(m_stopBtnArea.width, m_stopBtnArea.height));
    stopBox.setPosition(m_stopBtnArea.left, m_stopBtnArea.top);
    stopBox.setFillColor(sf::Color(150, 0, 0));
    window.draw(stopBox);
    sf::Text stopTxt("STOP", m_font, 11);
    stopTxt.setPosition(m_stopBtnArea.left + 10.f, m_stopBtnArea.top + 3.f);
    window.draw(stopTxt);

    // --- FPS CONTROL (- / +) ---
    float currentFps = 12.0f;
    if (!m_selectedAnimation.empty()) {
        auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
        if (anim) currentFps = anim->GetFPS();
    }

    m_fpsMinusBtnArea = sf::FloatRect(m_previewArea.left + 180.f, btnY, 20.f, 22.f);
    sf::RectangleShape minusBox(sf::Vector2f(20.f, 22.f));
    minusBox.setPosition(m_fpsMinusBtnArea.left, m_fpsMinusBtnArea.top);
    minusBox.setFillColor(sf::Color(80, 80, 80));
    window.draw(minusBox);
    sf::Text minusTxt("-", m_font, 14);
    minusTxt.setPosition(m_fpsMinusBtnArea.left + 6.f, m_fpsMinusBtnArea.top + 1.f);
    window.draw(minusTxt);

    sf::Text fpsDisplay(std::to_string(static_cast<int>(currentFps)) + " FPS", m_font, 12);
    fpsDisplay.setPosition(m_previewArea.left + 205.f, btnY + 3.f);
    window.draw(fpsDisplay);

    m_fpsPlusBtnArea = sf::FloatRect(m_previewArea.left + 265.f, btnY, 20.f, 22.f);
    sf::RectangleShape plusBox(sf::Vector2f(20.f, 22.f));
    plusBox.setPosition(m_fpsPlusBtnArea.left, m_fpsPlusBtnArea.top);
    plusBox.setFillColor(sf::Color(80, 80, 80));
    window.draw(plusBox);
    sf::Text plusTxt("+", m_font, 14);
    plusTxt.setPosition(m_fpsPlusBtnArea.left + 5.f, m_fpsPlusBtnArea.top + 1.f);
    window.draw(plusTxt);

    // --- SPRITE RENDER & GIZMOS ---
    auto spriteDef = engine.GetPlaybackEngine().GetCurrentSprite(engine.GetCurrentProject().get());
    auto texture = engine.GetCurrentTexture();
    
    if (spriteDef && texture) {
        sf::Texture gpuTex;
        gpuTex.create(texture->GetWidth(), texture->GetHeight());
        gpuTex.update(texture->GetPixels().data());
        
        sf::Sprite renderSprite;
        renderSprite.setTexture(gpuTex);
        const auto& r = spriteDef->GetSourceRect();
        renderSprite.setTextureRect(sf::IntRect(r.x, r.y, r.width, r.height));
        
        sf::Vector2f activeAnchor;
        float activeBaseline = 0.0f;
        
        if (autoAlign) {
            auto alignment = engine.GetPlaybackEngine().GetCurrentAlignment(engine.GetCurrentProject().get());
            activeAnchor.x = r.width * alignment.alignedPivot.x;
            activeAnchor.y = r.height * alignment.alignedPivot.y;
            activeBaseline = alignment.alignedBaseline;
        } else {
            activeAnchor.x = r.width * spriteDef->GetPivot().x;
            activeAnchor.y = r.height * spriteDef->GetPivot().y;
            activeBaseline = spriteDef->GetBaseline();
        }

        renderSprite.setOrigin(activeAnchor.x, activeAnchor.y);
        
        float scale = std::min((m_previewArea.width - 40.f) / r.width, (m_previewArea.height - 100.f) / r.height);
        renderSprite.setScale(scale, scale);
        
        sf::Vector2f previewCenter(m_previewArea.left + m_previewArea.width / 2.f, m_previewArea.top + 30.f + m_previewArea.height / 2.f);
        renderSprite.setPosition(previewCenter.x, previewCenter.y + (activeBaseline * scale));
        
        window.draw(renderSprite);

        // Gizmo Visualization
        sf::Vector2f activePos = renderSprite.getPosition();
        sf::Vector2f activeOrigin = renderSprite.getOrigin();

        sf::Vector2f tl;
        tl.x = activePos.x - (activeOrigin.x * scale);
        tl.y = activePos.y - (activeOrigin.y * scale);

        sf::RectangleShape activeBaselineLine(sf::Vector2f(m_previewArea.width, 1.f));
        activeBaselineLine.setPosition(m_previewArea.left, tl.y + (r.height - activeBaseline) * scale);
        activeBaselineLine.setFillColor(autoAlign ? sf::Color(0, 255, 255, 200) : sf::Color(255, 165, 0, 200));
        window.draw(activeBaselineLine);

        sf::CircleShape activePivotDot(4.f);
        activePivotDot.setOrigin(4.f, 4.f);
        activePivotDot.setPosition(activePos);
        activePivotDot.setFillColor(autoAlign ? sf::Color::Magenta : sf::Color::Green);
        window.draw(activePivotDot);

        if (autoAlign) {
            sf::RectangleShape origBaselineLine(sf::Vector2f(m_previewArea.width, 1.f));
            origBaselineLine.setPosition(m_previewArea.left, tl.y + (r.height - spriteDef->GetBaseline()) * scale);
            origBaselineLine.setFillColor(sf::Color(255, 165, 0, 100));
            window.draw(origBaselineLine);

            sf::CircleShape origPivotDot(4.f);
            origPivotDot.setOrigin(4.f, 4.f);
            origPivotDot.setPosition(tl.x + (r.width * spriteDef->GetPivot().x) * scale, tl.y + (r.height * spriteDef->GetPivot().y) * scale);
            origPivotDot.setFillColor(sf::Color(0, 255, 0, 100));
            window.draw(origPivotDot);
        }
    }
}

}