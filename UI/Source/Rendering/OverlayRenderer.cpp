#include "Rendering/OverlayRenderer.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace StudioUI {

OverlayRenderer::OverlayRenderer() = default;

bool OverlayRenderer::InitializeFont(const std::string& customPath) {
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
            std::cout << "[Overlay] Successfully loaded font: " << path << std::endl;
            return true;
        }
    }
    
    std::cerr << "[Overlay] FATAL: Could not load any system fonts for UI text rendering!" << std::endl;
    m_hasFont = false;
    return false;
}

void OverlayRenderer::DrawTextShadowed(sf::RenderWindow& window, const std::string& str, const sf::Vector2f& pos) {
    if (!m_hasFont) return;
    sf::Text text(str, m_font, 14);
    
    text.setPosition(pos.x + 1, pos.y + 1);
    text.setFillColor(sf::Color::Black);
    window.draw(text);

    text.setPosition(pos);
    text.setFillColor(sf::Color::White);
    window.draw(text);
}

void OverlayRenderer::RenderDebug(sf::RenderWindow& window, const DebugInfo& info) {
    std::ostringstream oss;
    oss << "--- DEBUG (F3) ---\n"
        << "FPS: " << static_cast<int>(info.fps) << " (" << std::fixed << std::setprecision(2) << info.frameTimeMs << " ms)\n"
        << "Zoom Level: " << std::fixed << std::setprecision(2) << info.zoomLevel << "x\n"
        << "Pan Offset: [" << static_cast<int>(info.panOffset.x) << ", " << static_cast<int>(info.panOffset.y) << "]\n"
        << "Viewport Size: " << static_cast<int>(info.viewportSize.x) << "x" << static_cast<int>(info.viewportSize.y) << "\n"
        << "Image Size: " << info.imageWidth << "x" << info.imageHeight << "\n"
        << "Project Status: " << (info.projectLoaded ? "Active" : "None") << "\n"
        << "Texture Status: " << (info.textureLoaded ? "Loaded" : "Empty") << "\n";

    if (info.selectionWidth > 0 && info.selectionHeight > 0) {
        oss << "Selection Size: " << static_cast<int>(info.selectionWidth) << "x" << static_cast<int>(info.selectionHeight) << " px\n";
    }

    DrawTextShadowed(window, oss.str(), sf::Vector2f(10.f, 10.f));
}

void OverlayRenderer::RenderInspector(sf::RenderWindow& window, const InspectorInfo& info) {
    std::ostringstream oss;
    oss << "--- MOUSE INSPECTOR ---\n"
        << "Screen Pos: [" << info.mouseWindow.x << ", " << info.mouseWindow.y << "]\n"
        << "Image Dimensions: " << info.imageWidth << "x" << info.imageHeight << "\n"
        << "Current Zoom: " << std::fixed << std::setprecision(2) << info.currentZoom << "x\n"
        << "Current Pan: [" << static_cast<int>(info.panOffset.x) << ", " << static_cast<int>(info.panOffset.y) << "]\n";
    
    if (info.isHoveringImage) {
        oss << "Image Pos: [" << info.mouseImage.x << ", " << info.mouseImage.y << "]\n"
            << "RGBA: (" << (int)info.pixelColor.r << ", " << (int)info.pixelColor.g << ", " 
                         << (int)info.pixelColor.b << ", " << (int)info.pixelColor.a << ")\n";
    } else {
        oss << "Image Pos: [OUT OF BOUNDS]\nRGBA: N/A\n";
    }

    sf::Vector2f pos(10.f, window.getSize().y - 140.f);
    DrawTextShadowed(window, oss.str(), pos);
}

void OverlayRenderer::RenderProgress(sf::RenderWindow& window, const JobProgressInfo& info) {
    if (!info.isRunning || !m_hasFont) return;
    
    sf::Vector2f center(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
    
    sf::RectangleShape bg(sf::Vector2f(300, 20));
    bg.setOrigin(150, 10);
    bg.setPosition(center);
    bg.setFillColor(sf::Color(50, 50, 50, 200));
    window.draw(bg);
    
    sf::RectangleShape fg(sf::Vector2f(300 * info.progress, 20));
    fg.setOrigin(150, 10);
    fg.setPosition(center);
    fg.setFillColor(sf::Color(0, 255, 0, 200));
    window.draw(fg);

    std::ostringstream oss;
    oss << "Detecting Sprites... " << static_cast<int>(info.progress * 100) << "% (Press ESC to Cancel)";
    DrawTextShadowed(window, oss.str(), sf::Vector2f(center.x - 140, center.y - 30));
}

void OverlayRenderer::RenderSpriteInspector(sf::RenderWindow& window, const SpriteInspectorInfo& info) {
    if (!info.isActive || !m_hasFont) return;
    
    std::ostringstream oss;
    oss << "--- SPRITE PROPERTIES ---\n"
        << "ID: " << info.id << "\n"
        << "Rect: " << info.x << ", " << info.y << "  [" << info.w << "x" << info.h << "]\n"
        << "Pixels: " << info.pixelCount << "\n"
        << "Center: " << std::fixed << std::setprecision(1) << info.cx << ", " << info.cy << "\n";
    
    DrawTextShadowed(window, oss.str(), sf::Vector2f(window.getSize().x - 250.f, 10.f));
}
}