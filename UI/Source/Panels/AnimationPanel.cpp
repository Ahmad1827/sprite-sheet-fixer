#include "Panels/AnimationPanel.h"
#include "Panels/PreviewViewport.h"
#include "StudioEngineFacade.h"
#include "DataModels/Project.h"
#include "DataModels/AnimationGroup.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/SourceTexture.h"
#include "Systems/PlaybackEngine.h"
#include "Theme.h"
#include "Widgets.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace StudioUI {

AnimationPanel::AnimationPanel() {
    m_bgShape.setFillColor(sf::Color(40, 40, 40, 255));
    m_bgShape.setOutlineColor(sf::Color(100, 100, 100));
    m_bgShape.setOutlineThickness(-1.0f);
}

bool AnimationPanel::InitializeFont(const std::string& customPath) {
    m_hasFont = LoadUiFont(m_font, customPath);
    return m_hasFont;
}

void AnimationPanel::SetBounds(const sf::FloatRect& bounds) {
    m_bounds = bounds;
}

void AnimationPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine, PreviewViewport& viewport) {
    if (!engine.IsProjectActive()) return;

    float timelineHeight = StudioUI::Theme::TimelineHeight;
    float rightPanelWidth = StudioUI::Theme::SidePanelWidth;
    float topOffset = StudioUI::Theme::ToolbarHeight; 
    
    const float usableHeight = m_bounds.height - m_bottomInset;
    m_timelineArea = sf::FloatRect(m_bounds.left, m_bounds.top + usableHeight - timelineHeight, m_bounds.width, timelineHeight);
    m_listArea = sf::FloatRect(m_bounds.left + m_bounds.width - rightPanelWidth, m_bounds.top + topOffset, rightPanelWidth, (usableHeight - topOffset - timelineHeight) / 2.0f);
    m_previewArea = sf::FloatRect(m_bounds.left + m_bounds.width - rightPanelWidth, m_listArea.top + m_listArea.height, rightPanelWidth, m_listArea.height);

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

            if (m_listArea.contains(mousePos)) {
                float yOffset = StudioUI::Theme::HeaderHeight + 10.f;
                auto project = engine.GetCurrentProject();
                for (const auto& anim : project->GetAnimationGroups()) {
                    sf::FloatRect itemRect(m_listArea.left + 10.f, m_listArea.top + yOffset, m_listArea.width - 20.f, 25.f);
                    if (itemRect.contains(mousePos)) {
                        m_selectedAnimation = anim->GetId();
                        engine.GetPlaybackEngine().SetActiveAnimation(m_selectedAnimation);
                        break;
                    }
                    yOffset += 30.f;
                }
            } 
            else if (m_timelineArea.contains(mousePos) && !m_selectedAnimation.empty()) {
                auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
                if (anim) {
                    if (!selectedSprites.empty()) {
                        auto frames = anim->GetFrames();
                        frames.insert(frames.end(), selectedSprites.begin(), selectedSprites.end());
                        engine.ModifyAnimationFrames(m_selectedAnimation, frames);
                        viewport.ClearSelection();
                    } else {
                        float timelineStartX = m_timelineArea.left + 270.0f;
                        float frameWidth = 32.0f;
                        int index = 0;
                        for (const auto& frame : anim->GetFrames()) {
                            sf::FloatRect frameRect(timelineStartX + (index * frameWidth), m_timelineArea.top + StudioUI::Theme::HeaderHeight + 30.f, frameWidth, 40.f);
                            if (frameRect.contains(mousePos)) {
                                m_draggedFrameIndex = index;
                                m_dropTargetIndex = index;
                                break;
                            }
                            index++;
                        }
                    }
                }
            }
        } 
        else if (event.mouseButton.button == sf::Mouse::Right && m_timelineArea.contains(mousePos) && !m_selectedAnimation.empty()) {
            auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
            if (anim) {
                float timelineStartX = m_timelineArea.left + 270.0f;
                float frameWidth = 32.0f;
                int index = 0;
                auto frames = anim->GetFrames();
                for (auto it = frames.begin(); it != frames.end(); ++it) {
                    sf::FloatRect frameRect(timelineStartX + (index * frameWidth), m_timelineArea.top + StudioUI::Theme::HeaderHeight + 30.f, frameWidth, 40.f);
                    if (frameRect.contains(mousePos)) {
                        frames.erase(it);
                        engine.ModifyAnimationFrames(m_selectedAnimation, frames);
                        break;
                    }
                    index++;
                }
            }
        }
    } 
    else if (event.type == sf::Event::MouseMoved && m_draggedFrameIndex != -1) {
        sf::Vector2f mousePos(event.mouseMove.x, event.mouseMove.y);
        if (m_timelineArea.contains(mousePos)) {
            float timelineStartX = m_timelineArea.left + 270.0f;
            float relX = mousePos.x - timelineStartX;
            m_dropTargetIndex = std::max(0, static_cast<int>(std::round(relX / 32.f)));
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
    if (!engine.IsProjectActive() || !m_hasFont) return;
    
    sf::View dynamicView(m_bounds);
    window.setView(dynamicView);

    float timelineHeight = StudioUI::Theme::TimelineHeight;
    float rightPanelWidth = StudioUI::Theme::SidePanelWidth;
    float topOffset = StudioUI::Theme::ToolbarHeight;
    
    const float usableHeight = m_bounds.height - m_bottomInset;
    m_timelineArea = sf::FloatRect(m_bounds.left, m_bounds.top + usableHeight - timelineHeight, m_bounds.width, timelineHeight);
    m_listArea = sf::FloatRect(m_bounds.left + m_bounds.width - rightPanelWidth, m_bounds.top + topOffset, rightPanelWidth, (usableHeight - topOffset - timelineHeight) / 2.0f);
    m_previewArea = sf::FloatRect(m_bounds.left + m_bounds.width - rightPanelWidth, m_listArea.top + m_listArea.height, rightPanelWidth, m_listArea.height);

    RenderList(window, engine);
    RenderPreview(window, engine);
    RenderTimeline(window, engine);
}

void AnimationPanel::RenderList(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    sf::RectangleShape bg(sf::Vector2f(m_listArea.width, m_listArea.height));
    bg.setPosition(m_listArea.left, m_listArea.top);
    bg.setFillColor(StudioUI::Theme::PanelBackground);
    bg.setOutlineThickness(StudioUI::Theme::BorderThickness);
    bg.setOutlineColor(StudioUI::Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header(sf::Vector2f(m_listArea.width, StudioUI::Theme::HeaderHeight));
    header.setPosition(m_listArea.left, m_listArea.top);
    header.setFillColor(StudioUI::Theme::InspectorBackground);
    window.draw(header);

    sf::Text title("ANIMATIONS     click one to select it", m_font, 11);
    title.setPosition(m_listArea.left + 12.f, m_listArea.top + 6.f);
    title.setFillColor(StudioUI::Theme::TextSecondary);
    window.draw(title);

    float yOffset = StudioUI::Theme::HeaderHeight + 10.f;
    auto project = engine.GetCurrentProject();
    int animCount = 0;
    
    for (const auto& anim : project->GetAnimationGroups()) {
        sf::RectangleShape item(sf::Vector2f(m_listArea.width - 20.f, 25.f));
        item.setPosition(m_listArea.left + 10.f, m_listArea.top + yOffset);
        item.setFillColor(anim->GetId() == m_selectedAnimation ? StudioUI::Theme::AccentColor : StudioUI::Theme::HoverColor);
        window.draw(item);

        sf::Text name(anim->GetName(), m_font, 12);
        name.setPosition(item.getPosition().x + 8.f, item.getPosition().y + 5.f);
        name.setFillColor(StudioUI::Theme::TextPrimary);
        window.draw(name);
        
        yOffset += 30.f;
        animCount++;
    }

    sf::Text countText("Total Animations: " + std::to_string(animCount), m_font, 11);
    countText.setPosition(m_listArea.left + 10.f, m_listArea.top + m_listArea.height - 20.f);
    countText.setFillColor(StudioUI::Theme::TextMuted);
    window.draw(countText);
}

void AnimationPanel::RenderPreview(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    sf::RectangleShape bg(sf::Vector2f(m_previewArea.width, m_previewArea.height));
    bg.setPosition(m_previewArea.left, m_previewArea.top);
    bg.setFillColor(StudioUI::Theme::PanelBackground);
    bg.setOutlineThickness(StudioUI::Theme::BorderThickness);
    bg.setOutlineColor(StudioUI::Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header(sf::Vector2f(m_previewArea.width, StudioUI::Theme::HeaderHeight));
    header.setPosition(m_previewArea.left, m_previewArea.top);
    header.setFillColor(StudioUI::Theme::InspectorBackground);
    window.draw(header);

    bool autoAlign = engine.IsAutoAlignEnabled();
    std::string titleStr = "PREVIEW     auto-align " + std::string(autoAlign ? "on" : "off") + "  (A)";
    sf::Text title(titleStr, m_font, 11);
    title.setPosition(m_previewArea.left + 12.f, m_previewArea.top + 6.f);
    title.setFillColor(autoAlign ? StudioUI::Theme::AccentColor : StudioUI::Theme::TextSecondary);
    window.draw(title);

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
    }
}

void AnimationPanel::RenderTimeline(sf::RenderWindow& window, const StudioCore::StudioEngineFacade& engine) {
    float posY = m_timelineArea.top;
    float panelHeight = m_timelineArea.height;
    
    sf::RectangleShape bg(sf::Vector2f(m_timelineArea.width, panelHeight));
    bg.setPosition(m_timelineArea.left, posY);
    bg.setFillColor(StudioUI::Theme::PanelBackground);
    bg.setOutlineThickness(StudioUI::Theme::BorderThickness);
    bg.setOutlineColor(StudioUI::Theme::BorderColor);
    window.draw(bg);

    sf::RectangleShape header(sf::Vector2f(m_timelineArea.width, StudioUI::Theme::HeaderHeight));
    header.setPosition(m_timelineArea.left, posY);
    header.setFillColor(StudioUI::Theme::InspectorBackground);
    window.draw(header);

    sf::Text headerText("TIMELINE     click here to add the selected sprites     drag a frame to reorder     right-click a frame to remove it", m_font, 11);
    headerText.setFillColor(StudioUI::Theme::TextSecondary);
    headerText.setPosition(m_timelineArea.left + 12.0f, posY + 6.0f);
    window.draw(headerText);

    float leftPanelWidth = 250.0f;
    sf::RectangleShape leftBorder(sf::Vector2f(StudioUI::Theme::BorderThickness, panelHeight - StudioUI::Theme::HeaderHeight));
    leftBorder.setPosition(m_timelineArea.left + leftPanelWidth, posY + StudioUI::Theme::HeaderHeight);
    leftBorder.setFillColor(StudioUI::Theme::BorderColor);
    window.draw(leftBorder);

    bool isPlaying = engine.GetPlaybackEngine().IsPlaying();
    float currentFps = 12.0f;
    int frameCount = 0;
    
    if (!m_selectedAnimation.empty()) {
        auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
        if (anim) {
            currentFps = anim->GetFPS();
            frameCount = anim->GetFrames().size();
        }
    }
    int currentFrame = engine.GetPlaybackEngine().GetCurrentFrameIndex();
    float ctrlY = 0.0f;

    ctrlY = posY + StudioUI::Theme::HeaderHeight + 14.0f;
    auto controlButton = [&](const sf::FloatRect& rect, const std::string& label, bool highlighted) {
        sf::Color fill = highlighted ? MixColor(StudioUI::Theme::PanelBackground, StudioUI::Theme::AccentColor, 0.32f) : StudioUI::Theme::HoverColor;
        DrawRoundedRect(window, rect, 5.0f, fill, highlighted ? StudioUI::Theme::AccentColor : StudioUI::Theme::BorderColor, 1.0f);
        sf::Text text(label, m_font, 12);
        text.setFillColor(highlighted ? sf::Color::White : StudioUI::Theme::TextPrimary);
        sf::FloatRect b = text.getLocalBounds();
        text.setPosition(std::floor(rect.left + (rect.width - b.width) / 2.0f - b.left),
                         std::floor(rect.top + (rect.height - b.height) / 2.0f - b.top));
        window.draw(text);
    };

    m_playBtnArea = sf::FloatRect(m_timelineArea.left + 16.f, ctrlY, 66.f, 26.f);
    m_pauseBtnArea = sf::FloatRect(m_timelineArea.left + 88.f, ctrlY, 66.f, 26.f);
    m_stopBtnArea = sf::FloatRect(m_timelineArea.left + 160.f, ctrlY, 66.f, 26.f);
    controlButton(m_playBtnArea, "Play", isPlaying);
    controlButton(m_pauseBtnArea, "Pause", false);
    controlButton(m_stopBtnArea, "Stop", false);

    m_fpsMinusBtnArea = sf::FloatRect(m_timelineArea.left + 16.f, ctrlY + 36.f, 30.f, 26.f);
    m_fpsPlusBtnArea = sf::FloatRect(m_timelineArea.left + 124.f, ctrlY + 36.f, 30.f, 26.f);
    controlButton(m_fpsMinusBtnArea, "-", false);
    controlButton(m_fpsPlusBtnArea, "+", false);

    sf::Text fpsTxt(std::to_string(static_cast<int>(currentFps)) + " fps", m_font, 12);
    fpsTxt.setFillColor(StudioUI::Theme::TextPrimary);
    sf::FloatRect fpsBounds = fpsTxt.getLocalBounds();
    fpsTxt.setPosition(std::floor(m_timelineArea.left + 85.f - fpsBounds.width / 2.0f), ctrlY + 41.f);
    window.draw(fpsTxt);

    sf::Text frameInfoTxt("Frame " + std::to_string(currentFrame + 1) + " of " + std::to_string(std::max(1, frameCount)), m_font, 11);
    frameInfoTxt.setFillColor(StudioUI::Theme::TextMuted);
    frameInfoTxt.setPosition(m_timelineArea.left + 16.f, ctrlY + 74.f);
    window.draw(frameInfoTxt);

    if (m_selectedAnimation.empty()) {
        sf::Text pick("Select an animation in the list on the right to see its frames here.", m_font, 12);
        pick.setFillColor(StudioUI::Theme::TextMuted);
        pick.setPosition(m_timelineArea.left + leftPanelWidth + 20.0f, posY + StudioUI::Theme::HeaderHeight + 44.0f);
        window.draw(pick);
    }

    if (m_selectedAnimation.empty()) return;
    auto anim = engine.GetCurrentProject()->GetAnimationById(m_selectedAnimation);
    if (!anim) return;

    float timelineStartX = m_timelineArea.left + leftPanelWidth + 20.0f;
    float timelineY = posY + StudioUI::Theme::HeaderHeight + 30.0f;
    float frameWidth = 32.0f;
    float frameHeight = 40.0f;
    
    float xOffset = timelineStartX;
    int index = 0;

    for (const auto& frameId : anim->GetFrames()) {
        sf::RectangleShape frameBox(sf::Vector2f(frameWidth - 2.0f, frameHeight));
        frameBox.setPosition(xOffset, timelineY);
        frameBox.setFillColor(index % 2 == 0 ? StudioUI::Theme::HoverColor : StudioUI::Theme::InspectorBackground);
        
        if (index == currentFrame) {
            frameBox.setOutlineThickness(1.5f);
            frameBox.setOutlineColor(StudioUI::Theme::AccentColor);
            frameBox.setFillColor(StudioUI::Theme::HoverColor);
        } else if (index == m_draggedFrameIndex) {
            frameBox.setOutlineThickness(2.f);
            frameBox.setOutlineColor(sf::Color::Yellow);
        }
        window.draw(frameBox);

        sf::Text tickText(std::to_string(index), m_font, 10);
        tickText.setFillColor(StudioUI::Theme::TextMuted);
        tickText.setPosition(xOffset + 2.0f, timelineY - 18.0f);
        window.draw(tickText);

        if (m_draggedFrameIndex != -1 && index == m_dropTargetIndex) {
            sf::RectangleShape dropMarker(sf::Vector2f(4.f, frameHeight + 20.f));
            dropMarker.setPosition(xOffset - 2.f, timelineY - 10.f);
            dropMarker.setFillColor(sf::Color::Cyan);
            window.draw(dropMarker);
        }

        xOffset += frameWidth;
        index++;
    }

    if (frameCount > 0 && currentFrame >= 0 && currentFrame < frameCount) {
        float playheadX = timelineStartX + (currentFrame * frameWidth) + (frameWidth / 2.0f);
        sf::RectangleShape playheadLine(sf::Vector2f(2.0f, frameHeight + 25.0f));
        playheadLine.setPosition(playheadX, timelineY - 15.0f);
        playheadLine.setFillColor(sf::Color(235, 87, 87));
        window.draw(playheadLine);
    }
}

}