#include "SpriteSheetStudioPanel.h"
#include "Utils/NativeFileDialog.h"
#include "DataModels/Project.h"

namespace StudioUI {

SpriteSheetStudioPanel::SpriteSheetStudioPanel() {
    m_animationPanel = std::make_unique<AnimationPanel>();
}

SpriteSheetStudioPanel::~SpriteSheetStudioPanel() = default;

void SpriteSheetStudioPanel::Initialize() {
    m_engine.Initialize();
    m_engine.CreateProject();
    m_viewport.Initialize();

    m_toolbar.Initialize("Resources/font.ttf",
        [this]() {
            std::string path = NativeFileDialog::OpenFileDialog();
            if (!path.empty()) LoadImage(path);
        },
        [this]() {
            std::string path = NativeFileDialog::OpenFileDialog("Sprite Sheet Studio Project (*.sps)");
            if (!path.empty()) {
                std::string err;
                if (m_engine.LoadProject(path, err)) m_viewport.RefreshTexture(m_engine);
            }
        },
        [this]() {
            std::string path = NativeFileDialog::SaveFileDialog("project.sps");
            if (!path.empty()) m_engine.SaveProject(path);
        },
        [this]() { m_isExportMode = true; m_exportPreview.Activate(m_engine); },
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

    m_animationPanel->InitializeFont("Resources/font.ttf");
    m_exportPreview.InitializeFont("Resources/font.ttf");
    m_animBuilderPanel.InitializeFont("Resources/font.ttf");
    m_workspace.InitializeFont("Resources/font.ttf");
}


void SpriteSheetStudioPanel::LoadImage(const std::string& filePath) {
    std::string errorMsg;
    if (m_engine.ImportImage(filePath, errorMsg)) {
        m_viewport.RefreshTexture(m_engine);
    }
}

void SpriteSheetStudioPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    if (event.type == sf::Event::KeyPressed) {
        bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
        if (isControl) {
            if (event.key.code == sf::Keyboard::Z) { m_engine.Undo(); m_viewport.RefreshTexture(m_engine); return; }
            if (event.key.code == sf::Keyboard::Y) { m_engine.Redo(); m_viewport.RefreshTexture(m_engine); return; }
            if (event.key.code == sf::Keyboard::E) { m_isExportMode = true; m_exportPreview.Activate(m_engine); return; }
            if (event.key.code == sf::Keyboard::S) {
                std::string path = NativeFileDialog::SaveFileDialog("project.sps");
                if (!path.empty()) m_engine.SaveProject(path);
                return;
            }
        }
    }

    if (m_workspace.HandleEvent(event, window)) return;

    if (m_isWizardMode) {
        bool exitWizard = false;
        m_animBuilderPanel.HandleEvent(event, window, m_engine, exitWizard);
        if (exitWizard) m_isWizardMode = false;
        return;
    }

    if (!m_isExportMode) {
        if (m_toolbar.HandleEvent(event, window, m_engine)) return;
    } else {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) m_isExportMode = false;
        else m_exportPreview.HandleEvent(event, window, m_engine);
        return;
    }

    m_viewport.HandleEvent(event, window, m_engine);
    if (m_animationPanel) m_animationPanel->HandleEvent(event, window, m_engine, m_viewport);
}

void SpriteSheetStudioPanel::Update(float deltaTime, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    if (!m_isExportMode && !m_isWizardMode) {
        m_engine.Update(deltaTime);
        m_viewport.Update(deltaTime);

        m_autoSaveTimer += deltaTime;
        if (m_autoSaveTimer >= 300.0f) {
            if (m_engine.IsProjectActive()) m_engine.SaveProject("autosave_backup.sps");
            m_autoSaveTimer = 0.0f;
        }

        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
        int totalSprites = (m_engine.IsProjectActive() && m_engine.GetCurrentProject())
                            ? static_cast<int>(m_engine.GetCurrentProject()->GetSprites().size()) : 0;

        m_workspace.UpdateStatusBar(1.0f, worldPos, totalSprites, 0, "Ready");
    }
}

void SpriteSheetStudioPanel::SetBounds(const sf::FloatRect& bounds) {
    m_bounds = bounds;
    m_toolbar.SetBounds(bounds);
    m_viewport.SetBounds(bounds);
    m_workspace.SetBounds(bounds);
    m_animBuilderPanel.SetBounds(bounds);
    if (m_animationPanel) {
        m_animationPanel->SetBounds(bounds);
    }
}

void SpriteSheetStudioPanel::Render(sf::RenderWindow& window) {
    if (!m_isActive) return;

    // Apply viewport view matching m_bounds so panels render correctly in sub-regions
    sf::View originalView = window.getView();
    sf::View panelView(m_bounds);
    window.setView(panelView);

    if (m_isExportMode) {
        m_exportPreview.Render(window);
    } else {
        m_viewport.Render(window, m_engine);
        if (!m_isUIHidden && m_animationPanel) m_animationPanel->Render(window, m_engine);
        m_toolbar.Render(window);
        if (m_isWizardMode) m_animBuilderPanel.Render(window);
        m_workspace.Render(window);
    }

    // Restore original view for host window
    window.setView(originalView);
}

} // namespace StudioUI