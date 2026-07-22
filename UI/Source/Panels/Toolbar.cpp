#include "Panels/Toolbar.h"
#include "StudioEngineFacade.h"
#include "Utils/NativeFileDialog.h"
#include <iostream>

namespace StudioUI {

Toolbar::Toolbar() {
    m_background.setSize({1280.f, 40.f});
    m_background.setFillColor(sf::Color(45, 45, 48));
}

void Toolbar::Initialize(const std::string& fontPath, std::function<void()> onOpenImage, std::function<void()> onLoadProject, std::function<void()> onToggleUI) {
    if (!m_font.loadFromFile(fontPath)) {
        if (!m_font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) return;
    }
    m_buttons.clear();
    float currentX = 10.f;

    AddButton(currentX, "Open Img", [onOpenImage](StudioCore::StudioEngineFacade& e) {
        if (e.HasTexture()) {
            std::cout << "[!] An image is already opened. Please save or create a new project before opening a new image." << std::endl;
            return;
        }
        onOpenImage();
    });
    currentX += 90.f;

    AddButton(currentX, "Load Proj", [onLoadProject](StudioCore::StudioEngineFacade& e) {
        if (e.HasTexture() || e.IsProjectActive()) {
            std::cout << "[!] A project or image is already active. Please restart or clear the workspace before loading another project." << std::endl;
            return;
        }
        onLoadProject();
    });
    currentX += 90.f;

    AddButton(currentX, "Save Proj", [](StudioCore::StudioEngineFacade& e) {
        if (!e.IsProjectActive()) return;
        std::string path = NativeFileDialog::SaveFileDialog("project.sps");
        if (!path.empty()) {
            if (path.find(".sps") == std::string::npos) path += ".sps";
            if (e.SaveProject(path)) std::cout << "[✓] Saved." << std::endl;
        }
    });
    currentX += 90.f;

    AddButton(currentX, "Detect", [](StudioCore::StudioEngineFacade& e) {
        if (e.HasTexture()) { StudioCore::DetectionConfig c; e.RunAutoDetection(c); }
    });
    currentX += 90.f;

    AddButton(currentX, "Export", [](StudioCore::StudioEngineFacade& e) {
        if (!e.IsProjectActive()) return;
        std::string path = NativeFileDialog::SaveFileDialog("aligned_spritesheet.png");
        if (!path.empty()) { e.ExportPNG(path, 8); }
    });
    currentX += 90.f;

    AddButton(currentX, "Undo", [](StudioCore::StudioEngineFacade& e) { e.Undo(); });
    currentX += 90.f;

    AddButton(currentX, "Redo", [](StudioCore::StudioEngineFacade& e) { e.Redo(); });
    currentX += 90.f;

    AddButton(currentX, "Toggle UI", [onToggleUI](StudioCore::StudioEngineFacade&) { onToggleUI(); });
    currentX += 90.f;
}

void Toolbar::AddButton(float x, const std::string& label, std::function<void(StudioCore::StudioEngineFacade&)> action) {
    Button b;
    b.rect.setPosition(x, 5.f);
    b.rect.setSize({80.f, 30.f});
    b.rect.setFillColor(sf::Color(60, 60, 65));
    b.rect.setOutlineColor(sf::Color(90, 90, 95));
    b.rect.setOutlineThickness(1.f);

    b.text.setFont(m_font);
    b.text.setString(label);
    b.text.setCharacterSize(12);
    b.text.setFillColor(sf::Color::White);

    sf::FloatRect textBounds = b.text.getLocalBounds();
    b.text.setPosition(x + (80.f - textBounds.width) / 2.f - textBounds.left, 5.f + (30.f - textBounds.height) / 2.f - textBounds.top);

    b.action = action;
    m_buttons.push_back(b);
}

bool Toolbar::HandleEvent(const sf::Event& event, const sf::RenderWindow& window, StudioCore::StudioEngineFacade& engine) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f pos(event.mouseButton.x, event.mouseButton.y);
        if (m_background.getGlobalBounds().contains(pos)) {
            for (auto& b : m_buttons) {
                if (b.rect.getGlobalBounds().contains(pos)) {
                    b.action(engine);
                }
            }
            return true; // Click consumed by toolbar!
        }
    }
    return false;
}

void Toolbar::Render(sf::RenderWindow& window) {
    window.draw(m_background);
    for (const auto& b : m_buttons) {
        window.draw(b.rect);
        window.draw(b.text);
    }
}

}