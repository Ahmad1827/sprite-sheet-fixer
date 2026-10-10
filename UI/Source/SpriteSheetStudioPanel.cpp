#include "SpriteSheetStudioPanel.h"
#include "Utils/NativeFileDialog.h"
#include "DataModels/Project.h"
#include "Processing/ImageLoader.h"
#include "DataModels/SpriteDefinition.h"
#include "Theme.h"
#include "Widgets.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdint>

#ifdef LoadImage
#undef LoadImage
#endif

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>

enum class DialogMode {
    OpenImage,
    SaveImage
};

static std::string openWindowsFileDialog(DialogMode mode, const char* defaultName = "spritesheet.png") {
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = { 0 };
    if (mode == DialogMode::SaveImage && defaultName) {
        strcpy_s(szFile, defaultName);
    }
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    const char imageFilter[] = "Image Files (*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp)\0*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrFilter = imageFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    std::string result = "";
    if (mode == DialogMode::OpenImage) {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            result = std::string(ofn.lpstrFile);
        }
    } else {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) {
            result = std::string(ofn.lpstrFile);
        }
    }
    SetCurrentDirectoryA(currentDir);
    return result;
}
#endif

static std::string CleanPath(std::string path) {
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ' || path.back() == '\t')) {
        path.pop_back();
    }
    while (!path.empty() && (path.front() == ' ' || path.front() == '\t')) {
        path.erase(path.begin());
    }
    return path;
}

static void ExportAtlasMetadata(StudioCore::StudioEngineFacade& engine, const std::string& baseFilePath) {
    if (!engine.IsProjectActive() || !engine.GetCurrentProject()) return;
    auto project = engine.GetCurrentProject();

    if (project->GetSprites().empty()) {
        StudioCore::DetectionConfig cfg;
        cfg.minSpriteSize = 10;
        engine.RunAutoDetection(cfg);
    }

    auto sprites = project->GetSprites();
    if (sprites.empty()) return;

    std::sort(sprites.begin(), sprites.end(), [](const std::shared_ptr<StudioCore::SpriteDefinition>& a, const std::shared_ptr<StudioCore::SpriteDefinition>& b) {
        auto ra = a->GetSourceRect();
        auto rb = b->GetSourceRect();
        if (std::abs(ra.y - rb.y) > 20) {
            return ra.y < rb.y;
        }
        return ra.x < rb.x;
    });

    namespace fs = std::filesystem;
    fs::path p(baseFilePath);
    std::string folder = p.parent_path().string();
    if (folder.empty()) folder = ".";
    std::string stem = p.stem().string();

    std::string atlasJsonPath = (fs::path(folder) / (stem + "-ATLAS.json")).string();
    std::string atlasTxtPath = (fs::path(folder) / (stem + "-ATLAS.txt")).string();

    std::ofstream jf(atlasJsonPath);
    if (jf.is_open()) {
        jf << "{\n";
        jf << "  \"exported_file\": \"" << stem << "\",\n";
        jf << "  \"total_sprites\": " << sprites.size() << ",\n";
        jf << "  \"sprites\": [\n";
        for (size_t i = 0; i < sprites.size(); ++i) {
            const auto& s = sprites[i];
            auto r = s->GetSourceRect();
            jf << "    {\n";
            jf << "      \"index\": " << (i + 1) << ",\n";
            jf << "      \"id\": \"" << s->GetId() << "\",\n";
            jf << "      \"x\": " << r.x << ",\n";
            jf << "      \"y\": " << r.y << ",\n";
            jf << "      \"width\": " << r.width << ",\n";
            jf << "      \"height\": " << r.height << "\n";
            jf << "    }" << (i + 1 < sprites.size() ? "," : "") << "\n";
        }
        jf << "  ]\n";
        jf << "}\n";
        jf.close();
    }

    std::ofstream tf(atlasTxtPath);
    if (tf.is_open()) {
        tf << "========================================================\n";
        tf << " ATLAS METADATA: " << stem << "\n";
        tf << "========================================================\n";
        tf << "INDEX | ID | X | Y | WIDTH | HEIGHT\n";
        tf << "--------------------------------------------------------\n";
        for (size_t i = 0; i < sprites.size(); ++i) {
            const auto& s = sprites[i];
            auto r = s->GetSourceRect();
            tf << (i + 1) << " | " << s->GetId() << " | X=" << r.x << " Y=" << r.y << " W=" << r.width << " H=" << r.height << "\n";
        }
        tf.close();
    }
}

namespace StudioUI {

SpriteSheetStudioPanel::SpriteSheetStudioPanel() {
    m_animationPanel = std::make_unique<AnimationPanel>();
}

SpriteSheetStudioPanel::~SpriteSheetStudioPanel() = default;

void SpriteSheetStudioPanel::Initialize() {
    m_engine.Initialize();
    m_engine.CreateProject();
    m_viewport.Initialize();

    if (!m_engine.IsAutoAlignEnabled()) {
        m_engine.ToggleAutoAlign();
    }

    m_settings = AppSettings::Load();

    auto hasImage = [this]() { return m_engine.HasTexture(); };
    auto hasSprites = [this]() { return SpriteCount() > 0; };
    auto next = [this](const char* id) { return [this, id]() { return NextStep() == id; }; };

    std::vector<ToolbarAction> actions;
    auto add = [&](ToolbarAction action) { actions.push_back(std::move(action)); };

    // id, label, group, tooltip, onClick, isActive, isEnabled, isPrimary
    add({ "open", u8"Open", u8"FILE", u8"Open an image (Ctrl+O). You can also drop a file on the window.",
          [this]() { ClearTools(); OpenImageDialog(); }, nullptr, nullptr, next("open") });

    add({ "clean", u8"Clean", u8"PREPARE", u8"Remove the background automatically. Ctrl+Z brings it back.",
          [this]() {
              ClearTools();
              StudioCore::CleanOptions options;
              options.softEdges = m_settings.cleanSoftEdges;
              options.removeFrameLines = m_settings.cleanRemoveFrameLines;
              options.maxSpeckArea = m_settings.cleanSpeckSize;
              m_engine.CleanCurrentTexture(options);
              m_cleanTried = true;
              m_viewport.RefreshTexture(m_engine);
          }, nullptr, hasImage, next("clean") });
    add({ "detect", u8"Detect", u8"PREPARE", u8"Find every sprite and draw a box around it.",
          [this]() { ClearTools(); RunDetect(); }, nullptr, hasImage, next("detect") });

    add({ "wand", u8"Wand", u8"FIX BY HAND", u8"Click a colour to select it, then delete or recolour it.",
          [this]() {
              bool enable = !m_isWandMode;
              ClearTools();
              m_isWandMode = enable;
          }, [this]() { return m_isWandMode; }, hasImage });
    add({ "delete", u8"Erase", u8"FIX BY HAND", u8"Drag a box to erase what is inside. Ctrl+click erases one colour patch.",
          [this]() {
              bool enable = !m_isDeleteMode;
              ClearTools();
              m_isDeleteMode = enable;
          }, [this]() { return m_isDeleteMode; }, hasImage });
    add({ "artifact", u8"Specks", u8"FIX BY HAND", u8"Click or drag a box to remove stray leftover pixels.",
          [this]() {
              bool enable = !m_isArtifactMode;
              ClearTools();
              m_isArtifactMode = enable;
          }, [this]() { return m_isArtifactMode; }, hasImage });
    add({ "infill", u8"Fill", u8"FIX BY HAND", u8"Drag a box over a sprite to fill see-through holes inside it.",
          [this]() {
              bool enable = !m_isInfillMode;
              ClearTools();
              m_isInfillMode = enable;
          }, [this]() { return m_isInfillMode; }, hasImage });

    add({ "review", u8"AI Review", u8"AI", u8"Let Claude Code find captions and stray bits to delete. Set it up in Settings.",
          [this]() { StartAiReview(); }, [this]() { return m_review.GetState() != ClaudeReview::State::Idle; },
          [this]() { return m_engine.HasTexture() && m_settings.aiEnabled; } });

    add({ "merge", u8"Merge", u8"SPRITES", u8"Join the selected sprite boxes into one. Select two or more first.",
          [this]() {
              ClearTools();
              auto selectedIds = m_viewport.GetSelectedSpriteIds();
              if (selectedIds.size() >= 2) {
                  m_engine.MergeSelectedSprites(selectedIds);
                  m_viewport.ClearSelection();
                  m_viewport.RefreshTexture(m_engine);
              }
          }, nullptr, [this]() { return m_viewport.GetSelectedSpriteIds().size() >= 2; } });
    add({ "repack", u8"Repack", u8"SPRITES", u8"Line the sprites up in one even row.",
          [this]() {
              ClearTools();
              m_engine.RepackFrames();
              m_viewport.RefreshTexture(m_engine);
          }, nullptr, hasImage });
    add({ "flip_h", u8"Flip", u8"SPRITES", u8"Mirror every sprite left to right.",
          [this]() {
              ClearTools();
              m_engine.FlipHorizontal();
              m_viewport.RefreshTexture(m_engine);
          }, nullptr, hasImage });

    add({ "wizard", u8"Animate", u8"ANIMATION", u8"Build animations automatically, one for each row of sprites.",
          [this]() {
              ClearTools();
              if (SpriteCount() == 0) return;
              m_isWizardMode = true;
              m_animBuilderPanel.Activate(m_engine);
          }, nullptr, hasSprites, next("animate") });
    add({ "timeline", u8"Timeline", u8"ANIMATION", u8"Show or hide the animation list, preview and timeline.",
          [this]() {
              if (AnimationCount() > 0) m_timelineHidden = !m_timelineHidden;
              else m_timelinePinned = !m_timelinePinned;
          }, [this]() { return AnimationPanelsVisible(); }, hasImage });

    ToolbarAction undo{ "undo", u8"Undo", u8"HISTORY", u8"Undo the last change (Ctrl+Z).",
          [this]() { m_engine.Undo(); m_viewport.ClearSelection(); m_viewport.RefreshTexture(m_engine); }, nullptr, hasImage };
    undo.alignRight = true;
    add(undo);
    ToolbarAction redo{ "redo", u8"Redo", u8"HISTORY", u8"Redo what you just undid (Ctrl+Y).",
          [this]() { m_engine.Redo(); m_viewport.ClearSelection(); m_viewport.RefreshTexture(m_engine); }, nullptr, hasImage };
    redo.alignRight = true;
    add(redo);

    ToolbarAction frames{ "export_frames", u8"Frames", u8"SAVE", u8"Save every sprite as its own PNG, plus a data file (Ctrl+S).",
          [this]() { ClearTools(); ExportFramesDialog(); }, nullptr, hasSprites };
    frames.alignRight = true;
    add(frames);
    ToolbarAction exportSheet{ "export", u8"Export", u8"SAVE", u8"Preview and save the finished sheet (Ctrl+E).",
          [this]() {
              ClearTools();
              m_isExportMode = true;
              m_exportPreview.Activate(m_engine);
          }, nullptr, hasImage, next("export") };
    exportSheet.alignRight = true;
    add(exportSheet);

    ToolbarAction settings{ "settings", u8"Settings", u8"APP", u8"Choose how Clean and the AI review behave.",
          [this]() { ClearTools(); m_settingsPanel.Open(m_settings); } };
    settings.alignRight = true;
    add(settings);

    m_toolbar.Initialize("Resources/font.ttf", std::move(actions));

    LoadUiFont(m_wandFont);
    m_animationPanel->InitializeFont("Resources/font.ttf");
    m_exportPreview.InitializeFont("Resources/font.ttf");
    m_animBuilderPanel.InitializeFont("Resources/font.ttf");
    m_workspace.InitializeFont("Resources/font.ttf");
    m_settingsPanel.InitializeFont("Resources/font.ttf");
}

// ---- AI review (Claude Code) ------------------------------------------------

void SpriteSheetStudioPanel::StartAiReview() {
    ClearTools();
    m_viewport.ClearSelection();
    m_review.Start(m_engine, m_settings, m_wandFont);
}

void SpriteSheetStudioPanel::ApplyAiReview() {
    auto regions = m_review.TakeMarked();
    if (!regions.empty()) {
        m_engine.EraseRegions(regions);
        m_viewport.ClearSelection();
        m_viewport.RefreshTexture(m_engine);
    }
}

sf::FloatRect SpriteSheetStudioPanel::ReviewBarRect() const {
    const float width = std::min(680.0f, m_bounds.width - 24.0f);
    return sf::FloatRect(std::floor(m_bounds.left + (m_bounds.width - width) / 2.0f),
                         m_bounds.top + m_bounds.height - Theme::StatusBarHeight - 66.0f, width, 52.0f);
}

sf::FloatRect SpriteSheetStudioPanel::ReviewCardRect() const {
    const float width = std::min(520.0f, m_bounds.width - 24.0f);
    const float height = 170.0f;
    return sf::FloatRect(std::floor(m_bounds.left + (m_bounds.width - width) / 2.0f),
                         std::floor(m_bounds.top + (m_bounds.height - height) / 2.0f), width, height);
}

namespace {
// Buttons inside the review bar and card, shared by drawing and clicking.
sf::FloatRect ReviewPrimaryButton(const sf::FloatRect& box) {
    return sf::FloatRect(box.left + box.width - 152.0f, box.top + box.height - 43.0f, 140.0f, 34.0f);
}
sf::FloatRect ReviewSecondaryButton(const sf::FloatRect& box) {
    return sf::FloatRect(box.left + box.width - 252.0f, box.top + box.height - 43.0f, 92.0f, 34.0f);
}
}

void SpriteSheetStudioPanel::HandleReviewEvent(const sf::Event& event, const sf::RenderWindow& window) {
    const auto state = m_review.GetState();

    if (event.type == sf::Event::MouseMoved) {
        m_reviewMouse = { static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) };
    }

    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            m_review.Cancel();
        } else if (event.key.code == sf::Keyboard::Enter) {
            if (state == ClaudeReview::State::Choosing) ApplyAiReview();
            else if (state == ClaudeReview::State::Failed) m_review.Cancel();
        }
        return;
    }

    if (state == ClaudeReview::State::Choosing) {
        // Looking around stays possible while choosing.
        const bool navigation = event.type == sf::Event::MouseWheelScrolled || event.type == sf::Event::MouseMoved ||
            ((event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased) &&
             event.mouseButton.button == sf::Mouse::Middle);
        if (navigation) {
            m_viewport.HandleEvent(event, window, m_engine);
            return;
        }
    }

    if (event.type != sf::Event::MouseButtonPressed || event.mouseButton.button != sf::Mouse::Left) return;
    const sf::Vector2f pos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));

    if (state == ClaudeReview::State::Choosing) {
        const sf::FloatRect bar = ReviewBarRect();
        if (ReviewPrimaryButton(bar).contains(pos)) {
            ApplyAiReview();
        } else if (ReviewSecondaryButton(bar).contains(pos)) {
            m_review.Cancel();
        } else if (!bar.contains(pos) && m_viewport.GetCanvasBounds().contains(pos)) {
            m_review.ToggleAt(m_viewport.MapPixelToWorld({ event.mouseButton.x, event.mouseButton.y }, window));
        }
        return;
    }

    const sf::FloatRect card = ReviewCardRect();
    if (state == ClaudeReview::State::Running) {
        if (ReviewPrimaryButton(card).contains(pos)) m_review.Cancel();
    } else if (state == ClaudeReview::State::Failed) {
        if (ReviewPrimaryButton(card).contains(pos)) {
            m_review.Cancel();
        } else if (ReviewSecondaryButton(card).contains(pos)) {
            m_review.Cancel();
            m_settingsPanel.Open(m_settings);
        }
    }
}

void SpriteSheetStudioPanel::RenderReview(sf::RenderWindow& window) {
    const auto state = m_review.GetState();
    if (state == ClaudeReview::State::Idle) return;

    const sf::View uiView(m_bounds);

    auto button = [&](const sf::FloatRect& rect, const std::string& label, bool primary, bool danger, bool enabled) {
        const bool hovered = enabled && rect.contains(m_reviewMouse);
        sf::Color fill = hovered ? Theme::BevelLight : Theme::HoverColor;
        sf::Color outline = Theme::BorderColor;
        sf::Color textColor = Theme::TextPrimary;
        if (!enabled) {
            fill = Theme::InspectorBackground;
            textColor = Theme::TextMuted;
        } else if (danger) {
            fill = hovered ? sf::Color(250, 110, 110) : Theme::DangerColor;
            outline = fill;
            textColor = sf::Color(30, 8, 8);
        } else if (primary) {
            fill = hovered ? Theme::AccentHoverColor : Theme::AccentColor;
            outline = fill;
            textColor = sf::Color(10, 18, 30);
        }
        DrawRoundedRect(window, rect, Theme::CornerRadius, fill, outline, 1.0f);
        sf::Text text(label, m_wandFont, 13);
        text.setFillColor(textColor);
        sf::FloatRect tb = text.getLocalBounds();
        text.setPosition(std::floor(rect.left + (rect.width - tb.width) / 2.0f - tb.left),
                         std::floor(rect.top + (rect.height - tb.height) / 2.0f - tb.top));
        window.draw(text);
    };

    if (state == ClaudeReview::State::Choosing) {
        // What will go is tinted red on the image itself.
        window.setView(m_viewport.GetView());
        const float px = std::max(0.0001f, m_viewport.GetZoom());
        const auto& regions = m_review.Regions();
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto& r = regions[i];
            const bool marked = m_review.IsMarked(i);
            sf::RectangleShape box({ static_cast<float>(r.width), static_cast<float>(r.height) });
            box.setPosition(static_cast<float>(r.x), static_cast<float>(r.y));
            box.setFillColor(marked ? sf::Color(235, 60, 60, 90) : sf::Color::Transparent);
            box.setOutlineThickness((marked ? 2.0f : 1.0f) * px);
            box.setOutlineColor(marked ? sf::Color(255, 90, 90) : sf::Color(150, 158, 172, 110));
            window.draw(box);
        }

        window.setView(uiView);
        const sf::FloatRect bar = ReviewBarRect();
        DrawRoundedRect(window, bar, 10.0f, sf::Color(18, 20, 24, 245), Theme::BorderColor, 1.0f);

        const int marked = m_review.MarkedCount();
        const std::string headline = marked == 0
            ? "Claude found nothing to remove."
            : "Claude suggests removing " + std::to_string(marked) + " of " + std::to_string(regions.size()) + " objects.";
        sf::Text title(headline, m_wandFont, 13);
        title.setFillColor(Theme::TextPrimary);
        title.setPosition(bar.left + 16.0f, bar.top + 9.0f);
        window.draw(title);

        sf::Text note("Click any box to switch it between keep and delete.", m_wandFont, 11);
        note.setFillColor(Theme::TextMuted);
        note.setPosition(bar.left + 16.0f, bar.top + 29.0f);
        window.draw(note);

        button(ReviewSecondaryButton(bar), "Cancel", false, false, true);
        button(ReviewPrimaryButton(bar), marked == 1 ? "Remove 1" : "Remove " + std::to_string(marked), false, true, marked > 0);
        return;
    }

    window.setView(uiView);
    sf::RectangleShape dim({ m_bounds.width, m_bounds.height });
    dim.setFillColor(sf::Color(0, 0, 0, 150));
    window.draw(dim);

    const sf::FloatRect card = ReviewCardRect();
    DrawRoundedRect(window, card, 12.0f, Theme::PanelBackground, Theme::BorderColor, 1.0f);

    if (state == ClaudeReview::State::Running) {
        sf::Text title("Claude is looking at the sheet...", m_wandFont, 18);
        title.setFillColor(Theme::TextPrimary);
        title.setPosition(card.left + 24.0f, card.top + 22.0f);
        window.draw(title);

        sf::Text note("This usually takes 5 to 20 seconds.     " + std::to_string(static_cast<int>(m_review.SecondsRunning())) + " s",
                      m_wandFont, 12);
        note.setFillColor(Theme::TextSecondary);
        note.setPosition(card.left + 24.0f, card.top + 54.0f);
        window.draw(note);

        // A simple moving bar so it is clear the app has not frozen.
        const float trackWidth = card.width - 48.0f;
        sf::RectangleShape track({ trackWidth, 4.0f });
        track.setPosition(card.left + 24.0f, card.top + 88.0f);
        track.setFillColor(Theme::InspectorBackground);
        window.draw(track);
        const float t = std::fmod(m_review.SecondsRunning() * 0.6f, 1.0f);
        sf::RectangleShape runner({ trackWidth * 0.25f, 4.0f });
        runner.setPosition(card.left + 24.0f + (trackWidth * 0.75f) * t, card.top + 88.0f);
        runner.setFillColor(Theme::AccentColor);
        window.draw(runner);

        button(ReviewPrimaryButton(card), "Cancel", false, false, true);
        return;
    }

    sf::Text title("The AI review did not work", m_wandFont, 18);
    title.setFillColor(Theme::TextPrimary);
    title.setPosition(card.left + 24.0f, card.top + 22.0f);
    window.draw(title);

    // Wrap the message to the card width.
    std::string wrapped;
    std::string lineText;
    std::istringstream words(m_review.GetError());
    std::string paragraph;
    sf::Text measure("", m_wandFont, 12);
    while (std::getline(words, paragraph)) {
        std::istringstream stream(paragraph);
        std::string word;
        lineText.clear();
        while (stream >> word) {
            const std::string candidate = lineText.empty() ? word : lineText + " " + word;
            measure.setString(candidate);
            if (measure.getLocalBounds().width > card.width - 48.0f && !lineText.empty()) {
                wrapped += lineText + "\n";
                lineText = word;
            } else {
                lineText = candidate;
            }
        }
        wrapped += lineText + "\n";
    }
    sf::Text message(wrapped, m_wandFont, 12);
    message.setFillColor(Theme::TextSecondary);
    message.setPosition(card.left + 24.0f, card.top + 54.0f);
    window.draw(message);

    button(ReviewSecondaryButton(card), "Settings", false, false, true);
    button(ReviewPrimaryButton(card), "OK", true, false, true);
}

void SpriteSheetStudioPanel::ClearTools() {
    m_isWandMode = false;
    m_isWandEyedropper = false;
    m_isDraggingWandTol = false;
    m_isDraggingWandHue = false;
    m_isDraggingWandSatVal = false;
    m_wandSelectionPixels.clear();
    m_isArtifactMode = false;
    m_isInfillMode = false;
    m_isDeleteMode = false;
    m_isDraggingArtifact = false;
}

void SpriteSheetStudioPanel::OpenImageDialog() {
#if defined(_WIN32)
    std::string path = openWindowsFileDialog(DialogMode::OpenImage);
#else
    std::string path = NativeFileDialog::OpenFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp)");
#endif
    path = CleanPath(path);
    if (!path.empty()) LoadImage(path);
}

void SpriteSheetStudioPanel::ExportFramesDialog() {
#if defined(_WIN32)
    std::string path = openWindowsFileDialog(DialogMode::SaveImage, "spritesheet.png");
#else
    std::string path = NativeFileDialog::SaveFileDialog("spritesheet.png");
#endif
    path = CleanPath(path);
    if (!path.empty() && m_engine.IsProjectActive()) {
        namespace fs = std::filesystem;
        fs::path p(path);
        std::string folder = p.parent_path().string();
        std::string base = p.stem().string();
        m_engine.ExportIndividualSprites(folder.empty() ? "." : folder, base);
        ExportAtlasMetadata(m_engine, path);
    }
}

void SpriteSheetStudioPanel::RunDetect() {
    if (!m_engine.IsProjectActive() || !m_engine.GetCurrentProject()) return;

    StudioCore::DetectionConfig config;
    config.minSpriteSize = 10;
    m_engine.RunAutoDetection(config);

    auto project = m_engine.GetCurrentProject();
    auto rawSprites = project->GetSprites();
    std::vector<StudioCore::Rect> rawRects;
    for (const auto& s : rawSprites) {
        rawRects.push_back(s->GetSourceRect());
    }

    std::vector<StudioCore::Rect> mainSprites;
    std::vector<StudioCore::Rect> fragments;
    for (const auto& r : rawRects) {
        if (r.width > 35 && r.height > 35) {
            mainSprites.push_back(r);
        } else {
            fragments.push_back(r);
        }
    }

    for (const auto& frag : fragments) {
        if (mainSprites.empty()) break;
        float minD = 999999.0f;
        int bestIdx = -1;
        float fcx = frag.x + frag.width / 2.0f;
        float fcy = frag.y + frag.height / 2.0f;
        for (size_t i = 0; i < mainSprites.size(); ++i) {
            float mcx = mainSprites[i].x + mainSprites[i].width / 2.0f;
            float mcy = mainSprites[i].y + mainSprites[i].height / 2.0f;
            float d = (fcx - mcx) * (fcx - mcx) + (fcy - mcy) * (fcy - mcy);
            if (d < minD) {
                minD = d;
                bestIdx = static_cast<int>(i);
            }
        }
        if (bestIdx != -1) {
            float minX = std::min(mainSprites[bestIdx].x, frag.x);
            float minY = std::min(mainSprites[bestIdx].y, frag.y);
            float maxX = std::max(mainSprites[bestIdx].x + mainSprites[bestIdx].width, frag.x + frag.width);
            float maxY = std::max(mainSprites[bestIdx].y + mainSprites[bestIdx].height, frag.y + frag.height);
            mainSprites[bestIdx].x = minX;
            mainSprites[bestIdx].y = minY;
            mainSprites[bestIdx].width = maxX - minX;
            mainSprites[bestIdx].height = maxY - minY;
        }
    }

    std::sort(mainSprites.begin(), mainSprites.end(), [](const StudioCore::Rect& a, const StudioCore::Rect& b) {
        if (std::abs(a.y - b.y) > 20) {
            return a.y < b.y;
        }
        return a.x < b.x; 
    });

    std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> newSprites;
    for (size_t i = 0; i < mainSprites.size(); ++i) {
        auto def = std::make_shared<StudioCore::SpriteDefinition>("sprite_" + std::to_string(i + 1), mainSprites[i]);
        newSprites.push_back(def);
    }

    m_engine.ReplaceSprites(newSprites);
    m_viewport.RefreshTexture(m_engine);
}

int SpriteSheetStudioPanel::SpriteCount() const {
    auto project = m_engine.GetCurrentProject();
    return project ? static_cast<int>(project->GetSprites().size()) : 0;
}

int SpriteSheetStudioPanel::AnimationCount() const {
    auto project = m_engine.GetCurrentProject();
    return project ? static_cast<int>(project->GetAnimationGroups().size()) : 0;
}

bool SpriteSheetStudioPanel::AnimationPanelsVisible() const {
    if (!m_engine.HasTexture()) return false;
    return AnimationCount() > 0 ? !m_timelineHidden : m_timelinePinned;
}

sf::FloatRect SpriteSheetStudioPanel::CanvasBounds() const {
    const bool panels = AnimationPanelsVisible();
    const float right = panels ? Theme::SidePanelWidth : 0.0f;
    const float bottom = Theme::StatusBarHeight + (panels ? Theme::TimelineHeight : 0.0f);
    // The wand panel floats on the left; keep the image out from under it.
    const float left = m_isWandMode ? 290.0f : 0.0f;
    return sf::FloatRect(m_bounds.left + left, m_bounds.top + Theme::ToolbarHeight,
                         std::max(1.0f, m_bounds.width - right - left),
                         std::max(1.0f, m_bounds.height - Theme::ToolbarHeight - bottom));
}

// True while most of the image border is still opaque, i.e. the background has
// not been removed yet.
bool SpriteSheetStudioPanel::BackgroundLooksOpaque() const {
    auto tex = m_engine.GetCurrentTexture();
    if (!tex || !tex->IsValid()) return false;
    const int w = tex->GetWidth();
    const int h = tex->GetHeight();
    const auto& pixels = tex->GetPixels();
    int opaque = 0;
    int total = 0;
    for (int i = 0; i < 32; ++i) {
        const int x = (w - 1) * i / 31;
        const int y = (h - 1) * i / 31;
        const size_t samples[4] = {
            static_cast<size_t>(x), static_cast<size_t>(h - 1) * w + x,
            static_cast<size_t>(y) * w, static_cast<size_t>(y) * w + (w - 1)
        };
        for (size_t sample : samples) {
            ++total;
            if (pixels[sample * 4 + 3] > 0) ++opaque;
        }
    }
    return opaque * 2 > total;
}

// The one button worth pressing next; the toolbar highlights it.
std::string SpriteSheetStudioPanel::NextStep() const {
    if (!m_engine.HasTexture()) return "open";
    if (SpriteCount() == 0) return (!m_cleanTried && BackgroundLooksOpaque()) ? "clean" : "detect";
    if (AnimationCount() == 0) return "animate";
    return "export";
}

std::string SpriteSheetStudioPanel::CurrentHint() const {
    if (!m_engine.HasTexture()) return u8"Open an image, or drop one on the window, to begin.";
    if (m_engine.IsDetectionRunning()) return u8"Detecting sprites...   Esc cancels.";
    if (m_review.GetState() == ClaudeReview::State::Running) return u8"Claude is looking at the sheet...   Esc cancels.";
    if (m_review.GetState() == ClaudeReview::State::Choosing) return u8"AI review: red boxes will be deleted. Click a box to switch it, then press Remove.   Esc cancels.";

    if (m_isWandMode) {
        if (m_isWandEyedropper) return u8"Wand: click a colour on the image to pick it up.";
        if (m_wandSelectionPixels.empty()) return u8"Wand: click a colour on the image to select it.   Esc closes the wand.";
        return u8"Wand: press Delete Selection or Recolor Selection in the panel.   Esc closes the wand.";
    }
    if (m_isDeleteMode) return u8"Erase: drag a box to erase everything inside.   Ctrl+click erases one colour patch.   Esc stops.";
    if (m_isArtifactMode) return u8"Specks: click or drag a box to remove stray leftover pixels.   Esc stops.";
    if (m_isInfillMode) return u8"Fill: drag a box over a sprite to fill see-through holes inside it.   Esc stops.";

    const size_t selected = m_viewport.GetSelectedSpriteIds().size();
    if (selected > 0) {
        return std::to_string(selected) + " selected.     Del deletes     Merge joins     Alt+drag moves the pivot     Right-click for more";
    }

    const std::string step = NextStep();
    const std::string navigation = "          Scroll to zoom     Middle mouse to pan     F to fit";
    if (step == "clean") return u8"Step 1: press Clean to remove the background." + navigation;
    if (step == "detect") return u8"Step 2: press Detect to find the sprites." + navigation;
    if (step == "animate") return u8"Step 3: press Animate to group the sprites into animations, or Export to save." + navigation;
    return u8"Step 4: press Export to save the sheet.   Space plays the selected animation." + navigation;
}

void SpriteSheetStudioPanel::LoadImage(const std::string& filePath) {
    std::string clean = CleanPath(filePath);
    std::string errorMsg;
    if (m_engine.ImportImage(clean, errorMsg)) {
        ClearTools();
        m_cleanTried = false;
        m_viewport.SetCanvasBounds(CanvasBounds());
        m_viewport.RefreshTexture(m_engine);
    }
}

void SpriteSheetStudioPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::FloatRect currentBounds(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    SetBounds(currentBounds);

    // The settings window and a running AI review take every event.
    if (m_settingsPanel.IsOpen()) {
        m_settingsPanel.HandleEvent(event);
        return;
    }
    if (m_review.GetState() != ClaudeReview::State::Idle) {
        HandleReviewEvent(event, window);
        return;
    }

    if (event.type == sf::Event::KeyPressed) {
        bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
        bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

        if (event.key.code == sf::Keyboard::Delete) {
            auto selectedIds = m_viewport.GetSelectedSpriteIds();
            if (!selectedIds.empty()) {
                m_engine.BeginUndoGroup();
                for (const auto& id : selectedIds) {
                    m_engine.DeleteSpriteWithPixels(id);
                }
                m_engine.EndUndoGroup();
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
                return;
            }
        }

        if (isShift && event.key.code == sf::Keyboard::N) {
            if (m_engine.IsProjectActive()) {
                static int animCounter = 1;
                m_engine.CreateAnimation("New Animation " + std::to_string(animCounter++));
                m_viewport.RefreshTexture(m_engine);
                return;
            }
        }
        if (isControl) {
            if (event.key.code == sf::Keyboard::Z) {
                if (isShift) {
                    m_engine.Redo();
                } else {
                    m_engine.Undo();
                }
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
                return;
            }
            if (event.key.code == sf::Keyboard::Y) {
                m_engine.Redo();
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
                return;
            }
            if (event.key.code == sf::Keyboard::E) {
                m_isExportMode = true;
                m_exportPreview.Activate(m_engine);
                return;
            }
            if (event.key.code == sf::Keyboard::L || event.key.code == sf::Keyboard::O) {
#if defined(_WIN32)
                std::string path = openWindowsFileDialog(DialogMode::OpenImage);
#else
                std::string path = NativeFileDialog::OpenFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp)");
#endif
                path = CleanPath(path);
                if (!path.empty()) {
                    LoadImage(path);
                }
                return;
            }
            if (event.key.code == sf::Keyboard::S) {
#if defined(_WIN32)
                std::string path = openWindowsFileDialog(DialogMode::SaveImage, "spritesheet.png");
#else
                std::string path = NativeFileDialog::SaveFileDialog("spritesheet.png");
#endif
                path = CleanPath(path);
                if (!path.empty() && m_engine.IsProjectActive()) {
                    namespace fs = std::filesystem;
                    fs::path p(path);
                    std::string folder = p.parent_path().string();
                    std::string base = p.stem().string();
                    m_engine.ExportIndividualSprites(folder.empty() ? "." : folder, base);
                    ExportAtlasMetadata(m_engine, path);
                }
                return;
            }
        }
        if (event.key.code == sf::Keyboard::A && !isControl) {
            m_engine.ToggleAutoAlign();
            m_viewport.RefreshTexture(m_engine);
            return;
        }
        if (event.key.code == sf::Keyboard::Escape) {
            m_isWandMode = false;
            m_isWandEyedropper = false;
            m_wandSelectionPixels.clear();
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            m_isDraggingArtifact = false;
        }
    }

    if (m_workspace.HandleEvent(event, window)) return;

    if (m_isWizardMode) {
        bool exitWizard = false;
        m_animBuilderPanel.HandleEvent(event, window, m_engine, exitWizard);
        if (exitWizard) m_isWizardMode = false;
        return;
    }

    if (m_isExportMode) {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            m_exportPreview.Deactivate();
            m_isExportMode = false;
        } else {
            m_exportPreview.HandleEvent(event, window, m_engine);
            if (!m_exportPreview.IsActive()) {
                m_isExportMode = false;
                std::string lastPath = m_exportPreview.GetLastExportPath();
                if (!lastPath.empty()) {
                    ExportAtlasMetadata(m_engine, lastPath);
                }
            }
        }
        return;
    }

    if (m_toolbar.HandleEvent(event, window)) {
        m_viewport.RefreshTexture(m_engine);
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        if (m_isWandMode) {
            float pw = 260.0f;
            float ph = 410.0f;
            float px0 = 18.0f;
            float py0 = Theme::ToolbarHeight + 12.0f;
            sf::FloatRect panelRect(px0, py0, pw, ph);

            float mx = static_cast<float>(event.mouseButton.x);
            float my = static_cast<float>(event.mouseButton.y);

            if (panelRect.contains(mx, my)) {
                if (sf::FloatRect(px0 + pw - 24.0f, py0 + 6.0f, 18.0f, 18.0f).contains(mx, my)) {
                    m_isWandMode = false;
                    m_isWandEyedropper = false;
                    m_wandSelectionPixels.clear();
                    return;
                }

                if (sf::FloatRect(px0 + 12.0f, py0 + 52.0f, 26.0f, 20.0f).contains(mx, my)) {
                    m_wandTolerance = std::max(0.0f, m_wandTolerance - 4.0f);
                    return;
                }
                if (sf::FloatRect(px0 + pw - 38.0f, py0 + 52.0f, 26.0f, 20.0f).contains(mx, my)) {
                    m_wandTolerance = std::min(255.0f, m_wandTolerance + 4.0f);
                    return;
                }
                sf::FloatRect sliderRect(px0 + 44.0f, py0 + 54.0f, pw - 88.0f, 16.0f);
                if (sliderRect.contains(mx, my)) {
                    m_isDraggingWandTol = true;
                    float t = std::clamp((mx - sliderRect.left) / sliderRect.width, 0.0f, 1.0f);
                    m_wandTolerance = t * 128.0f;
                    return;
                }

                sf::FloatRect contigTab(px0 + 12.0f, py0 + 78.0f, 114.0f, 22.0f);
                sf::FloatRect globalTab(px0 + 134.0f, py0 + 78.0f, 114.0f, 22.0f);
                if (contigTab.contains(mx, my)) {
                    m_wandContiguous = true;
                    return;
                }
                if (globalTab.contains(mx, my)) {
                    m_wandContiguous = false;
                    return;
                }

                sf::FloatRect delTab(px0 + 12.0f, py0 + 104.0f, 114.0f, 24.0f);
                sf::FloatRect recTab(px0 + 134.0f, py0 + 104.0f, 114.0f, 24.0f);
                if (delTab.contains(mx, my)) {
                    m_wandActionDelete = true;
                    return;
                }
                if (recTab.contains(mx, my)) {
                    m_wandActionDelete = false;
                    return;
                }

                if (!m_wandActionDelete) {
                    sf::FloatRect svBox(px0 + 12.0f, py0 + 136.0f, pw - 24.0f, 110.0f);
                    if (svBox.contains(mx, my)) {
                        m_isDraggingWandSatVal = true;
                        m_wandSat = std::clamp((mx - svBox.left) / svBox.width, 0.0f, 1.0f);
                        m_wandVal = std::clamp(1.0f - ((my - svBox.top) / svBox.height), 0.0f, 1.0f);
                        UpdateWandColorFromHsv();
                        return;
                    }

                    sf::FloatRect hueBar(px0 + 12.0f, py0 + 254.0f, pw - 24.0f, 16.0f);
                    if (hueBar.contains(mx, my)) {
                        m_isDraggingWandHue = true;
                        float t = std::clamp((mx - hueBar.left) / hueBar.width, 0.0f, 1.0f);
                        m_wandHue = t * 360.0f;
                        UpdateWandColorFromHsv();
                        return;
                    }

                    sf::FloatRect eyeBtn(px0 + 12.0f, py0 + 278.0f, pw - 24.0f, 24.0f);
                    if (eyeBtn.contains(mx, my)) {
                        m_isWandEyedropper = !m_isWandEyedropper;
                        return;
                    }
                }

                float applyY = m_wandActionDelete ? (py0 + 138.0f) : (py0 + 310.0f);
                sf::FloatRect applyBtn(px0 + 12.0f, applyY, pw - 24.0f, 30.0f);
                if (applyBtn.contains(mx, my)) {
                    ApplyWandAction();
                    return;
                }

                sf::FloatRect clearBtn(px0 + 12.0f, applyY + 36.0f, pw - 24.0f, 22.0f);
                if (clearBtn.contains(mx, my)) {
                    m_wandSelectionPixels.clear();
                    return;
                }

                return;
            }

            sf::Vector2i clickPixel(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f worldPos = window.mapPixelToCoords(clickPixel, m_viewport.GetView());
            int px = static_cast<int>(std::floor(worldPos.x));
            int py = static_cast<int>(std::floor(worldPos.y));

            if (m_engine.HasTexture() && m_engine.GetCurrentTexture()) {
                auto tex = m_engine.GetCurrentTexture();
                int width = tex->GetWidth();
                int height = tex->GetHeight();

                if (px >= 0 && px < width && py >= 0 && py < height) {
                    const auto& rawPixels = tex->GetPixels();
                    size_t targetIdx = static_cast<size_t>(py * width + px) * 4;

                    if (m_isWandEyedropper) {
                        m_wandSelectedColor = sf::Color(rawPixels[targetIdx], rawPixels[targetIdx + 1], rawPixels[targetIdx + 2], 255);
                        m_isWandEyedropper = false;
                        m_wandActionDelete = false;

                        float r = m_wandSelectedColor.r / 255.0f;
                        float g = m_wandSelectedColor.g / 255.0f;
                        float b = m_wandSelectedColor.b / 255.0f;
                        float maxC = std::max({r, g, b});
                        float minC = std::min({r, g, b});
                        float delta = maxC - minC;
                        m_wandVal = maxC;
                        m_wandSat = (maxC > 0.0001f) ? (delta / maxC) : 0.0f;
                        if (delta < 0.0001f) {
                            m_wandHue = 0.0f;
                        } else if (maxC == r) {
                            m_wandHue = 60.0f * std::fmod((g - b) / delta, 6.0f);
                            if (m_wandHue < 0.0f) m_wandHue += 360.0f;
                        } else if (maxC == g) {
                            m_wandHue = 60.0f * (((b - r) / delta) + 2.0f);
                        } else {
                            m_wandHue = 60.0f * (((r - g) / delta) + 4.0f);
                        }
                        return;
                    }

                    if (rawPixels[targetIdx + 3] > 0) {
                        m_wandSelectionPixels = m_engine.SelectPixelsByColor(px, py, m_wandTolerance, m_wandContiguous);
                        return;
                    }
                }
            }
            return;
        }

        bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
        if (isControl && (m_isArtifactMode || m_isDeleteMode)) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, window);
            int px = static_cast<int>(std::floor(worldPos.x));
            int py = static_cast<int>(std::floor(worldPos.y));

            if (m_engine.HasTexture() && m_engine.GetCurrentTexture()) {
                auto tex = m_engine.GetCurrentTexture();
                int width = tex->GetWidth();
                int height = tex->GetHeight();

                if (px >= 0 && px < width && py >= 0 && py < height) {
                    size_t targetIdx = static_cast<size_t>(py * width + px) * 4;

                    if (tex->GetPixels()[targetIdx + 3] > 0) {
                        if (m_isDeleteMode) {
                            m_engine.DeleteColorRegion(px, py);
                        } else if (m_isArtifactMode) {
                            m_engine.RemoveArtifacts(px, py);
                        }

                        m_viewport.RefreshTexture(m_engine);
                        return;
                    }
                }
            }
        }

        if (m_isArtifactMode || m_isInfillMode || m_isDeleteMode) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, window);

            m_artifactDragStart = worldPos;
            m_artifactDragCurrent = worldPos;
            m_dragStartPixel = pixelPos;
            m_dragCurrentPixel = pixelPos;
            m_isDraggingArtifact = true;
            return;
        }
    }

    if (event.type == sf::Event::MouseMoved) {
        if (m_isWandMode) {
            float pw = 260.0f;
            float px0 = 18.0f;
            float py0 = Theme::ToolbarHeight + 12.0f;
            float mx = static_cast<float>(event.mouseMove.x);
            float my = static_cast<float>(event.mouseMove.y);

            if (m_isDraggingWandTol) {
                sf::FloatRect sliderRect(px0 + 44.0f, py0 + 54.0f, pw - 88.0f, 16.0f);
                float t = std::clamp((mx - sliderRect.left) / sliderRect.width, 0.0f, 1.0f);
                m_wandTolerance = t * 128.0f;
                return;
            }
            if (m_isDraggingWandHue) {
                sf::FloatRect hueBar(px0 + 12.0f, py0 + 254.0f, pw - 24.0f, 16.0f);
                float t = std::clamp((mx - hueBar.left) / hueBar.width, 0.0f, 1.0f);
                m_wandHue = t * 360.0f;
                UpdateWandColorFromHsv();
                return;
            }
            if (m_isDraggingWandSatVal) {
                sf::FloatRect svBox(px0 + 12.0f, py0 + 136.0f, pw - 24.0f, 110.0f);
                m_wandSat = std::clamp((mx - svBox.left) / svBox.width, 0.0f, 1.0f);
                m_wandVal = std::clamp(1.0f - ((my - svBox.top) / svBox.height), 0.0f, 1.0f);
                UpdateWandColorFromHsv();
                return;
            }
        }

        if (m_isDraggingArtifact) {
            sf::Vector2i pixelPos(event.mouseMove.x, event.mouseMove.y);
            m_artifactDragCurrent = m_viewport.MapPixelToWorld(pixelPos, window);
            m_dragCurrentPixel = pixelPos;
            return;
        }
    }

    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
        m_isDraggingWandTol = false;
        m_isDraggingWandHue = false;
        m_isDraggingWandSatVal = false;

        if (m_isDraggingArtifact) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            sf::Vector2f dragEnd = m_viewport.MapPixelToWorld(pixelPos, window);

            float minX = std::min(m_artifactDragStart.x, dragEnd.x);
            float maxX = std::max(m_artifactDragStart.x, dragEnd.x);
            float minY = std::min(m_artifactDragStart.y, dragEnd.y);
            float maxY = std::max(m_artifactDragStart.y, dragEnd.y);

            if (m_isInfillMode) {
                m_engine.FillTransparencyArea(
                    static_cast<int>(minX),
                    static_cast<int>(minY),
                    std::max(1, static_cast<int>(maxX - minX)),
                    std::max(1, static_cast<int>(maxY - minY))
                );
            } else if (m_isDeleteMode) {
                if (std::abs(maxX - minX) < 2.0f && std::abs(maxY - minY) < 2.0f) {
                    m_engine.DeleteArea(static_cast<int>(minX), static_cast<int>(minY), 1, 1);
                } else {
                    m_engine.DeleteArea(
                        static_cast<int>(minX),
                        static_cast<int>(minY),
                        static_cast<int>(maxX - minX),
                        static_cast<int>(maxY - minY)
                    );
                }
            } else {
                if (std::abs(maxX - minX) < 2.0f && std::abs(maxY - minY) < 2.0f) {
                    m_engine.RemoveArtifacts(static_cast<int>(minX), static_cast<int>(minY));
                } else {
                    m_engine.RemoveArtifactsArea(
                        static_cast<int>(minX),
                        static_cast<int>(minY),
                        static_cast<int>(maxX - minX),
                        static_cast<int>(maxY - minY)
                    );
                }
            }

            m_viewport.RefreshTexture(m_engine);
            m_isDraggingArtifact = false;
            return;
        }
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
        sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
        sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, window);
        std::string targetSpriteId = "";
        if (m_engine.IsProjectActive() && m_engine.GetCurrentProject()) {
            auto sprites = m_engine.GetCurrentProject()->GetSprites();
            for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
                auto sprite = *it;
                if (!sprite) continue;
                auto rect = sprite->GetSourceRect();
                sf::FloatRect bounds(rect.x, rect.y, rect.width, rect.height);
                if (bounds.contains(worldPos)) {
                    targetSpriteId = sprite->GetId();
                    break;
                }
            }
        }
        if (!targetSpriteId.empty()) {
            auto selectedIds = m_viewport.GetSelectedSpriteIds();
            std::vector<std::string> targetsToDelete;
            if (std::find(selectedIds.begin(), selectedIds.end(), targetSpriteId) != selectedIds.end()) {
                targetsToDelete = selectedIds;
            } else {
                targetsToDelete = { targetSpriteId };
            }

            std::string deleteLabel = targetsToDelete.size() > 1 
                ? "Delete " + std::to_string(targetsToDelete.size()) + " Sprites" 
                : "Delete Sprite";
            std::string pivotLabel = targetsToDelete.size() > 1
                ? "Reset Pivots (" + std::to_string(targetsToDelete.size()) + ")"
                : "Reset Pivot";

            std::vector<StudioUI::ContextMenuItem> items = {
                {deleteLabel, [this, targetsToDelete]() {
                    m_engine.BeginUndoGroup();
                    for (const auto& id : targetsToDelete) {
                        m_engine.DeleteSpriteWithPixels(id);
                    }
                    m_engine.EndUndoGroup();
                    m_viewport.ClearSelection();
                    m_viewport.RefreshTexture(m_engine);
                }},
                {pivotLabel, [this, targetsToDelete]() {
                    auto proj = m_engine.GetCurrentProject();
                    if (!proj) return;
                    m_engine.BeginUndoGroup();
                    for (const auto& id : targetsToDelete) {
                        auto sprite = proj->GetSpriteById(id);
                        if (sprite) {
                            auto rect = sprite->GetSourceRect();
                            m_engine.EditPivot({id}, {rect.width / 2.0f, rect.height / 2.0f});
                        }
                    }
                    m_engine.EndUndoGroup();
                    m_viewport.RefreshTexture(m_engine);
                }}
            };
            m_workspace.ShowContextMenu({static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y)}, items);
            return;
        }
    }

    if (m_animationPanel && AnimationPanelsVisible()) {
        m_animationPanel->HandleEvent(event, window, m_engine, m_viewport);
    }
    m_viewport.HandleEvent(event, window, m_engine);
}

void SpriteSheetStudioPanel::Update(float deltaTime, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::FloatRect currentBounds(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    SetBounds(currentBounds);

    if (m_isExportMode) {
        if (!m_exportPreview.IsActive()) {
            m_isExportMode = false;
        }
        return;
    }
    m_review.Update();
    if (m_review.GetState() == ClaudeReview::State::Choosing && !m_settings.aiAskFirst) {
        ApplyAiReview();
    }

    if (!m_isWizardMode) {
        m_engine.Update(deltaTime);
        m_viewport.Update(deltaTime);
        m_wandAntsOffset += deltaTime * 24.0f;

        std::string info;
        if (m_engine.HasTexture()) {
            auto tex = m_engine.GetCurrentTexture();
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldPos = m_viewport.MapPixelToWorld(pixelPos, window);
            const int ix = static_cast<int>(std::floor(worldPos.x));
            const int iy = static_cast<int>(std::floor(worldPos.y));
            if (ix >= 0 && iy >= 0 && ix < tex->GetWidth() && iy < tex->GetHeight() &&
                m_viewport.GetCanvasBounds().contains(static_cast<float>(pixelPos.x), static_cast<float>(pixelPos.y))) {
                info += std::to_string(ix) + ", " + std::to_string(iy) + u8"      ";
            }
            info += std::to_string(tex->GetWidth()) + " x " + std::to_string(tex->GetHeight()) + u8" px";
            info += u8"      " + std::to_string(SpriteCount()) + u8" sprites";
            const float zoom = std::max(0.0001f, m_viewport.GetZoom());
            info += u8"      Zoom " + std::to_string(static_cast<int>(100.0f / zoom + 0.5f)) + u8"%";
        }
        m_workspace.SetStatus(CurrentHint(), info);
    }
}

void SpriteSheetStudioPanel::SetBounds(const sf::FloatRect& bounds) {
    m_bounds = bounds;
    m_toolbar.SetBounds(bounds);
    m_viewport.SetBounds(bounds);
    m_viewport.SetCanvasBounds(CanvasBounds());
    m_workspace.SetBounds(bounds);
    m_animBuilderPanel.SetBounds(bounds);
    m_settingsPanel.SetBounds(bounds);

    if (m_animationPanel) {
        // The panels sit above the status bar instead of underneath it.
        m_animationPanel->SetBounds(bounds);
        m_animationPanel->SetBottomInset(Theme::StatusBarHeight);
    }
}

void SpriteSheetStudioPanel::Render(sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::FloatRect currentBounds(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    SetBounds(currentBounds);

    sf::View uiView(currentBounds);
    window.setView(uiView);

    sf::RectangleShape backdrop({ currentBounds.width, currentBounds.height });
    backdrop.setFillColor(Theme::MainBackground);
    window.draw(backdrop);

    if (m_isExportMode && m_exportPreview.IsActive()) {
        m_exportPreview.Render(window);
    } else {
        m_isExportMode = false;
        m_viewport.Render(window, m_engine);

        if (m_isWandMode && !m_wandSelectionPixels.empty()) {
            window.setView(m_viewport.GetView());

            sf::VertexArray fillMesh(sf::Quads);
            int minX = 999999, maxX = -999999, minY = 999999, maxY = -999999;
            sf::Color selTint(0, 190, 255, 110);

            for (const auto& p : m_wandSelectionPixels) {
                float fx = static_cast<float>(p.x);
                float fy = static_cast<float>(p.y);
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);

                fillMesh.append(sf::Vertex(sf::Vector2f(fx, fy), selTint));
                fillMesh.append(sf::Vertex(sf::Vector2f(fx + 1.0f, fy), selTint));
                fillMesh.append(sf::Vertex(sf::Vector2f(fx + 1.0f, fy + 1.0f), selTint));
                fillMesh.append(sf::Vertex(sf::Vector2f(fx, fy + 1.0f), selTint));
            }
            window.draw(fillMesh);

            sf::FloatRect boundsRect(static_cast<float>(minX), static_cast<float>(minY),
                                     static_cast<float>(maxX - minX + 1), static_cast<float>(maxY - minY + 1));
            DrawDashedBox(window, boundsRect, m_wandAntsOffset, m_viewport.GetZoom());
        }

        window.setView(uiView);

        if (m_isDraggingArtifact) {
            float minX = std::min(m_dragStartPixel.x, m_dragCurrentPixel.x);
            float minY = std::min(m_dragStartPixel.y, m_dragCurrentPixel.y);
            float width = std::abs(m_dragCurrentPixel.x - m_dragStartPixel.x);
            float height = std::abs(m_dragCurrentPixel.y - m_dragStartPixel.y);

            sf::RectangleShape selectionRect(sf::Vector2f(width, height));
            selectionRect.setPosition(minX, minY);
            if (m_isInfillMode) {
                selectionRect.setFillColor(sf::Color(80, 220, 120, 80));
                selectionRect.setOutlineColor(sf::Color(100, 255, 150));
            } else if (m_isDeleteMode) {
                selectionRect.setFillColor(sf::Color(255, 50, 50, 80));
                selectionRect.setOutlineColor(sf::Color(255, 80, 80));
            } else {
                selectionRect.setFillColor(sf::Color(50, 150, 255, 80));
                selectionRect.setOutlineColor(sf::Color(80, 180, 255));
            }
            selectionRect.setOutlineThickness(1.0f);
            
            window.draw(selectionRect);
        }

        if (m_animationPanel && AnimationPanelsVisible()) {
            m_animationPanel->Render(window, m_engine);
        }
        m_toolbar.Render(window);
        if (m_isWizardMode) {
            m_animBuilderPanel.Render(window);
        }
        m_workspace.Render(window);

        if (m_isWandMode) {
            float pw = 260.0f;
            float ph = m_wandActionDelete ? 254.0f : 426.0f;
            float px0 = 18.0f;
            float py0 = Theme::ToolbarHeight + 12.0f;

            sf::RectangleShape pbg({pw, ph});
            pbg.setPosition(px0, py0);
            pbg.setFillColor(sf::Color(20, 16, 26, 245));
            pbg.setOutlineThickness(1.0f);
            pbg.setOutlineColor(Theme::BorderColor);
            window.draw(pbg);

            sf::RectangleShape pHeader({pw, 26.0f});
            pHeader.setPosition(px0, py0);
            pHeader.setFillColor(Theme::PanelBackground);
            window.draw(pHeader);

            sf::Text titleText(u8"Magic Wand", m_wandFont, 12);
            titleText.setFillColor(Theme::AccentColor);
            titleText.setPosition(px0 + 10.0f, py0 + 5.0f);
            window.draw(titleText);

            sf::Text closeBtnText(u8"X", m_wandFont, 12);
            closeBtnText.setFillColor(Theme::TextMuted);
            closeBtnText.setPosition(px0 + pw - 18.0f, py0 + 5.0f);
            window.draw(closeBtnText);

            sf::Text tolLabel(u8"Tolerance: " + std::to_string(static_cast<int>(m_wandTolerance)), m_wandFont, 11);
            tolLabel.setFillColor(Theme::TextPrimary);
            tolLabel.setPosition(px0 + 12.0f, py0 + 34.0f);
            window.draw(tolLabel);

            sf::RectangleShape btnMinus({26.0f, 20.0f});
            btnMinus.setPosition(px0 + 12.0f, py0 + 52.0f);
            btnMinus.setFillColor(Theme::PanelBackground);
            btnMinus.setOutlineThickness(1.0f);
            btnMinus.setOutlineColor(Theme::BorderColor);
            window.draw(btnMinus);
            sf::Text mText(u8"-", m_wandFont, 12);
            mText.setFillColor(Theme::TextPrimary);
            mText.setPosition(px0 + 21.0f, py0 + 53.0f);
            window.draw(mText);

            sf::RectangleShape sliderTrack({pw - 88.0f, 10.0f});
            sliderTrack.setPosition(px0 + 44.0f, py0 + 57.0f);
            sliderTrack.setFillColor(sf::Color(10, 8, 14));
            window.draw(sliderTrack);

            float fillFrac = std::clamp(m_wandTolerance / 128.0f, 0.0f, 1.0f);
            sf::RectangleShape sliderFill({(pw - 88.0f) * fillFrac, 10.0f});
            sliderFill.setPosition(px0 + 44.0f, py0 + 57.0f);
            sliderFill.setFillColor(Theme::AccentColor);
            window.draw(sliderFill);

            sf::RectangleShape btnPlus({26.0f, 20.0f});
            btnPlus.setPosition(px0 + pw - 38.0f, py0 + 52.0f);
            btnPlus.setFillColor(Theme::PanelBackground);
            btnPlus.setOutlineThickness(1.0f);
            btnPlus.setOutlineColor(Theme::BorderColor);
            window.draw(btnPlus);
            sf::Text pText(u8"+", m_wandFont, 12);
            pText.setFillColor(Theme::TextPrimary);
            pText.setPosition(px0 + pw - 30.0f, py0 + 53.0f);
            window.draw(pText);

            sf::RectangleShape tabContig({114.0f, 22.0f});
            tabContig.setPosition(px0 + 12.0f, py0 + 78.0f);
            tabContig.setFillColor(m_wandContiguous ? Theme::AccentColor : Theme::PanelBackground);
            tabContig.setOutlineThickness(1.0f);
            tabContig.setOutlineColor(m_wandContiguous ? Theme::AccentHoverColor : Theme::BorderColor);
            window.draw(tabContig);
            sf::Text contigText(u8"Contiguous", m_wandFont, 11);
            contigText.setFillColor(sf::Color::White);
            contigText.setPosition(px0 + 36.0f, py0 + 81.0f);
            window.draw(contigText);

            sf::RectangleShape tabGlobal({114.0f, 22.0f});
            tabGlobal.setPosition(px0 + 134.0f, py0 + 78.0f);
            tabGlobal.setFillColor(!m_wandContiguous ? Theme::AccentColor : Theme::PanelBackground);
            tabGlobal.setOutlineThickness(1.0f);
            tabGlobal.setOutlineColor(!m_wandContiguous ? Theme::AccentHoverColor : Theme::BorderColor);
            window.draw(tabGlobal);
            sf::Text globalText(u8"Global", m_wandFont, 11);
            globalText.setFillColor(sf::Color::White);
            globalText.setPosition(px0 + 172.0f, py0 + 81.0f);
            window.draw(globalText);

            sf::RectangleShape tabDel({114.0f, 24.0f});
            tabDel.setPosition(px0 + 12.0f, py0 + 104.0f);
            tabDel.setFillColor(m_wandActionDelete ? sf::Color(190, 45, 45) : Theme::PanelBackground);
            tabDel.setOutlineThickness(1.0f);
            tabDel.setOutlineColor(m_wandActionDelete ? sf::Color(255, 80, 80) : Theme::BorderColor);
            window.draw(tabDel);
            sf::Text delText(u8"Delete", m_wandFont, 11);
            delText.setFillColor(sf::Color::White);
            delText.setPosition(px0 + 48.0f, py0 + 108.0f);
            window.draw(delText);

            sf::RectangleShape tabRec({114.0f, 24.0f});
            tabRec.setPosition(px0 + 134.0f, py0 + 104.0f);
            tabRec.setFillColor(!m_wandActionDelete ? Theme::AccentColor : Theme::PanelBackground);
            tabRec.setOutlineThickness(1.0f);
            tabRec.setOutlineColor(!m_wandActionDelete ? Theme::AccentHoverColor : Theme::BorderColor);
            window.draw(tabRec);
            sf::Text recText(u8"Recolor", m_wandFont, 11);
            recText.setFillColor(sf::Color::White);
            recText.setPosition(px0 + 168.0f, py0 + 108.0f);
            window.draw(recText);

            if (!m_wandActionDelete) {
                sf::VertexArray svMesh(sf::Quads, 4);
                float svW = pw - 24.0f;
                float svH = 110.0f;
                float svX = px0 + 12.0f;
                float svY = py0 + 136.0f;

                float c = 1.0f;
                float x = c * (1.0f - std::abs(std::fmod(m_wandHue / 60.0f, 2.0f) - 1.0f));
                float r = 0, g = 0, b = 0;
                if (m_wandHue < 60.0f) { r = c; g = x; b = 0; }
                else if (m_wandHue < 120.0f) { r = x; g = c; b = 0; }
                else if (m_wandHue < 180.0f) { r = 0; g = c; b = x; }
                else if (m_wandHue < 240.0f) { r = 0; g = x; b = c; }
                else if (m_wandHue < 300.0f) { r = x; g = 0; b = c; }
                else { r = c; g = 0; b = x; }
                sf::Color pureHue(static_cast<sf::Uint8>(r * 255.f), static_cast<sf::Uint8>(g * 255.f), static_cast<sf::Uint8>(b * 255.f));

                svMesh[0] = sf::Vertex(sf::Vector2f(svX, svY), sf::Color::White);
                svMesh[1] = sf::Vertex(sf::Vector2f(svX + svW, svY), pureHue);
                svMesh[2] = sf::Vertex(sf::Vector2f(svX + svW, svY + svH), sf::Color::Black);
                svMesh[3] = sf::Vertex(sf::Vector2f(svX, svY + svH), sf::Color::Black);
                window.draw(svMesh);

                sf::RectangleShape svFrame({svW, svH});
                svFrame.setPosition(svX, svY);
                svFrame.setFillColor(sf::Color::Transparent);
                svFrame.setOutlineThickness(1.0f);
                svFrame.setOutlineColor(Theme::BorderColor);
                window.draw(svFrame);

                float pinX = svX + m_wandSat * svW;
                float pinY = svY + (1.0f - m_wandVal) * svH;
                sf::CircleShape pin(4.0f);
                pin.setOrigin(4.0f, 4.0f);
                pin.setPosition(pinX, pinY);
                pin.setFillColor(m_wandSelectedColor);
                pin.setOutlineThickness(1.5f);
                pin.setOutlineColor(sf::Color::White);
                window.draw(pin);

                float hbX = px0 + 12.0f;
                float hbY = py0 + 254.0f;
                float hbW = pw - 24.0f;
                float hbH = 16.0f;

                const sf::Color hueStops[7] = {
                    sf::Color::Red, sf::Color::Yellow, sf::Color::Green,
                    sf::Color::Cyan, sf::Color::Blue, sf::Color::Magenta, sf::Color::Red
                };
                sf::VertexArray hueMesh(sf::Quads, 24);
                float step = hbW / 6.0f;
                for (int i = 0; i < 6; ++i) {
                    float x0 = hbX + i * step;
                    float x1 = hbX + (i + 1) * step;
                    hueMesh[i * 4 + 0] = sf::Vertex(sf::Vector2f(x0, hbY), hueStops[i]);
                    hueMesh[i * 4 + 1] = sf::Vertex(sf::Vector2f(x1, hbY), hueStops[i + 1]);
                    hueMesh[i * 4 + 2] = sf::Vertex(sf::Vector2f(x1, hbY + hbH), hueStops[i + 1]);
                    hueMesh[i * 4 + 3] = sf::Vertex(sf::Vector2f(x0, hbY + hbH), hueStops[i]);
                }
                window.draw(hueMesh);

                sf::RectangleShape hbFrame({hbW, hbH});
                hbFrame.setPosition(hbX, hbY);
                hbFrame.setFillColor(sf::Color::Transparent);
                hbFrame.setOutlineThickness(1.0f);
                hbFrame.setOutlineColor(Theme::BorderColor);
                window.draw(hbFrame);

                float huePinX = hbX + (m_wandHue / 360.0f) * hbW;
                sf::RectangleShape huePin({4.0f, hbH + 4.0f});
                huePin.setOrigin(2.0f, 2.0f);
                huePin.setPosition(huePinX, hbY);
                huePin.setFillColor(sf::Color::White);
                huePin.setOutlineThickness(1.0f);
                huePin.setOutlineColor(sf::Color::Black);
                window.draw(huePin);

                sf::RectangleShape eyeBtn({pw - 24.0f, 24.0f});
                eyeBtn.setPosition(px0 + 12.0f, py0 + 278.0f);
                eyeBtn.setFillColor(m_isWandEyedropper ? sf::Color(45, 140, 75) : Theme::PanelBackground);
                eyeBtn.setOutlineThickness(1.0f);
                eyeBtn.setOutlineColor(m_isWandEyedropper ? sf::Color(80, 220, 120) : Theme::BorderColor);
                window.draw(eyeBtn);

                sf::RectangleShape curBox({14.0f, 14.0f});
                curBox.setPosition(px0 + 18.0f, py0 + 283.0f);
                curBox.setFillColor(m_wandSelectedColor);
                curBox.setOutlineThickness(1.0f);
                curBox.setOutlineColor(sf::Color::White);
                window.draw(curBox);

                sf::Text eyeText(m_isWandEyedropper ? u8"Click Canvas to Sample" : u8"Eyedrop from Canvas", m_wandFont, 11);
                eyeText.setFillColor(sf::Color::White);
                eyeText.setPosition(px0 + 40.0f, py0 + 282.0f);
                window.draw(eyeText);
            }

            float applyY = m_wandActionDelete ? (py0 + 138.0f) : (py0 + 310.0f);
            sf::RectangleShape applyBtn({pw - 24.0f, 30.0f});
            applyBtn.setPosition(px0 + 12.0f, applyY);
            applyBtn.setFillColor(m_wandActionDelete ? sf::Color(180, 40, 40) : Theme::AccentColor);
            applyBtn.setOutlineThickness(1.0f);
            applyBtn.setOutlineColor(Theme::AccentHoverColor);
            window.draw(applyBtn);

            std::string applyStr = m_wandActionDelete ? u8"Delete Selection" : u8"Recolor Selection";
            sf::Text aText(applyStr, m_wandFont, 11);
            aText.setFillColor(sf::Color::White);
            aText.setPosition(px0 + 44.0f, applyY + 7.0f);
            window.draw(aText);

            sf::RectangleShape clearBtn({pw - 24.0f, 22.0f});
            clearBtn.setPosition(px0 + 12.0f, applyY + 36.0f);
            clearBtn.setFillColor(Theme::PanelBackground);
            clearBtn.setOutlineThickness(1.0f);
            clearBtn.setOutlineColor(Theme::BorderColor);
            window.draw(clearBtn);

            sf::Text cText(u8"Deselect All", m_wandFont, 10);
            cText.setFillColor(Theme::TextMuted);
            cText.setPosition(px0 + 88.0f, applyY + 40.0f);
            window.draw(cText);

            std::string countStr = std::to_string(m_wandSelectionPixels.size()) + u8" pixels selected";
            sf::Text countText(countStr, m_wandFont, 10);
            countText.setFillColor(Theme::TextMuted);
            countText.setPosition(px0 + 14.0f, applyY + 64.0f);
            window.draw(countText);
        }
    }

    window.setView(uiView);
    RenderReview(window);
    window.setView(uiView);
    m_settingsPanel.Render(window);
}

void SpriteSheetStudioPanel::UpdateWandColorFromHsv() {
    float c = m_wandVal * m_wandSat;
    float x = c * (1.0f - std::abs(std::fmod(m_wandHue / 60.0f, 2.0f) - 1.0f));
    float m = m_wandVal - c;
    float r = 0, g = 0, b = 0;
    if (m_wandHue < 60.0f) { r = c; g = x; b = 0; }
    else if (m_wandHue < 120.0f) { r = x; g = c; b = 0; }
    else if (m_wandHue < 180.0f) { r = 0; g = c; b = x; }
    else if (m_wandHue < 240.0f) { r = 0; g = x; b = c; }
    else if (m_wandHue < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    m_wandSelectedColor = sf::Color(
        static_cast<sf::Uint8>(std::clamp((r + m) * 255.0f, 0.0f, 255.0f)),
        static_cast<sf::Uint8>(std::clamp((g + m) * 255.0f, 0.0f, 255.0f)),
        static_cast<sf::Uint8>(std::clamp((b + m) * 255.0f, 0.0f, 255.0f)),
        255
    );
}

void SpriteSheetStudioPanel::ApplyWandAction() {
    if (m_wandSelectionPixels.empty()) return;
    if (!m_engine.HasTexture() || !m_engine.GetCurrentTexture()) return;

    if (m_wandActionDelete) {
        m_engine.ErasePixels(m_wandSelectionPixels);
    } else {
        m_engine.RecolorPixels(m_wandSelectionPixels, m_wandSelectedColor);
    }

    m_wandSelectionPixels.clear();
    m_viewport.RefreshTexture(m_engine);
}

void SpriteSheetStudioPanel::DrawDashedBox(sf::RenderWindow& window, sf::FloatRect rect, float offset, float zoom) {
    float invZoom = std::max(0.1f, zoom);
    float thick = 1.0f / invZoom;
    float segLen = 4.0f / invZoom;

    auto drawDashedLine = [&](sf::Vector2f p1, sf::Vector2f p2) {
        sf::Vector2f d = p2 - p1;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 0.001f) return;
        sf::Vector2f dir = d / len;

        float cur = std::fmod(offset / invZoom, segLen * 2.0f);
        if (cur > 0.0f) cur -= segLen * 2.0f;

        while (cur < len) {
            float s0 = std::max(0.0f, cur);
            float s1 = std::min(len, cur + segLen);
            if (s1 > s0) {
                sf::Vertex line[2] = {
                    sf::Vertex(p1 + dir * s0, sf::Color::White),
                    sf::Vertex(p1 + dir * s1, sf::Color::White)
                };
                window.draw(line, 2, sf::Lines);
            }
            cur += segLen * 2.0f;
        }
    };

    sf::RectangleShape baseBox({rect.width, rect.height});
    baseBox.setPosition(rect.left, rect.top);
    baseBox.setFillColor(sf::Color::Transparent);
    baseBox.setOutlineThickness(thick);
    baseBox.setOutlineColor(sf::Color(15, 10, 20, 220));
    window.draw(baseBox);

    sf::Vector2f tl(rect.left, rect.top);
    sf::Vector2f tr(rect.left + rect.width, rect.top);
    sf::Vector2f br(rect.left + rect.width, rect.top + rect.height);
    sf::Vector2f bl(rect.left, rect.top + rect.height);

    drawDashedLine(tl, tr);
    drawDashedLine(tr, br);
    drawDashedLine(br, bl);
    drawDashedLine(bl, tl);
}

}