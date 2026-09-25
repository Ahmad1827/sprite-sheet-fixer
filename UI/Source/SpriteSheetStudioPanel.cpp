#include "SpriteSheetStudioPanel.h"
#include "Utils/NativeFileDialog.h"
#include "DataModels/Project.h"
#include "Processing/ImageLoader.h"
#include "DataModels/SpriteDefinition.h"
#include "Theme.h"
#include <algorithm>
#include <queue>
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

struct EditorHistoryState {
    std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> sprites;
    std::vector<uint8_t> pixels;
    int width{0};
    int height{0};
};

static std::vector<EditorHistoryState> g_undoStack;
static std::vector<EditorHistoryState> g_redoStack;

static void PushUndoState(StudioCore::StudioEngineFacade& engine) {
    if (!engine.IsProjectActive() || !engine.GetCurrentProject()) return;
    auto project = engine.GetCurrentProject();
    
    EditorHistoryState state;
    for (const auto& s : project->GetSprites()) {
        if (!s) continue;
        auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
        copy->SetPivot(s->GetPivot());
        state.sprites.push_back(copy);
    }

    if (engine.HasTexture() && engine.GetCurrentTexture()) {
        auto tex = engine.GetCurrentTexture();
        state.width = tex->GetWidth();
        state.height = tex->GetHeight();
        state.pixels = tex->GetPixels();
    }

    g_undoStack.push_back(state);
    g_redoStack.clear();
}

static void PerformUndo(StudioCore::StudioEngineFacade& engine) {
    if (!g_undoStack.empty() && engine.IsProjectActive() && engine.GetCurrentProject()) {
        auto project = engine.GetCurrentProject();
        
        EditorHistoryState currentCloned;
        for (const auto& s : project->GetSprites()) {
            if (!s) continue;
            auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
            copy->SetPivot(s->GetPivot());
            currentCloned.sprites.push_back(copy);
        }
        if (engine.HasTexture() && engine.GetCurrentTexture()) {
            auto tex = engine.GetCurrentTexture();
            currentCloned.width = tex->GetWidth();
            currentCloned.height = tex->GetHeight();
            currentCloned.pixels = tex->GetPixels();
        }
        g_redoStack.push_back(currentCloned);

        auto prevState = g_undoStack.back();
        g_undoStack.pop_back();

        project->SetSprites(prevState.sprites);

        if (engine.HasTexture() && engine.GetCurrentTexture() && !prevState.pixels.empty()) {
            auto tex = engine.GetCurrentTexture();
            if (tex->GetWidth() == prevState.width && tex->GetHeight() == prevState.height) {
                auto& rawPixels = const_cast<std::vector<uint8_t>&>(tex->GetPixels());
                rawPixels = prevState.pixels;
            }
        }
    }
    engine.Undo();
}

static void PerformRedo(StudioCore::StudioEngineFacade& engine) {
    if (!g_redoStack.empty() && engine.IsProjectActive() && engine.GetCurrentProject()) {
        auto project = engine.GetCurrentProject();
        
        EditorHistoryState currentCloned;
        for (const auto& s : project->GetSprites()) {
            if (!s) continue;
            auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
            copy->SetPivot(s->GetPivot());
            currentCloned.sprites.push_back(copy);
        }
        if (engine.HasTexture() && engine.GetCurrentTexture()) {
            auto tex = engine.GetCurrentTexture();
            currentCloned.width = tex->GetWidth();
            currentCloned.height = tex->GetHeight();
            currentCloned.pixels = tex->GetPixels();
        }
        g_undoStack.push_back(currentCloned);

        auto nextState = g_redoStack.back();
        g_redoStack.pop_back();

        project->SetSprites(nextState.sprites);

        if (engine.HasTexture() && engine.GetCurrentTexture() && !nextState.pixels.empty()) {
            auto tex = engine.GetCurrentTexture();
            if (tex->GetWidth() == nextState.width && tex->GetHeight() == nextState.height) {
                auto& rawPixels = const_cast<std::vector<uint8_t>&>(tex->GetPixels());
                rawPixels = nextState.pixels;
            }
        }
    }
    engine.Redo();
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

    m_toolbar.Initialize("Resources/font.ttf",
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
#if defined(_WIN32)
            std::string path = openWindowsFileDialog(DialogMode::OpenImage);
#else
            std::string path = NativeFileDialog::OpenFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp)");
#endif
            path = CleanPath(path);
            if (!path.empty()) LoadImage(path);
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
#if defined(_WIN32)
            std::string path = openWindowsFileDialog(DialogMode::OpenImage);
#else
            std::string path = NativeFileDialog::OpenFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.webp)");
#endif
            path = CleanPath(path);
            if (!path.empty()) LoadImage(path);
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
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
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            m_isExportMode = true; 
            m_exportPreview.Activate(m_engine); 
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
#if defined(_WIN32)
            std::string path = openWindowsFileDialog(DialogMode::SaveImage, "frame.png");
#else
            std::string path = NativeFileDialog::SaveFileDialog("frame.png");
#endif
            path = CleanPath(path);
            if (!path.empty()) {
                namespace fs = std::filesystem;
                fs::path p(path);
                std::string folder = p.parent_path().string();
                std::string base = p.stem().string();
                m_engine.ExportIndividualSprites(folder.empty() ? "." : folder, base);
                ExportAtlasMetadata(m_engine, path);
            }
        },
        [this]() {
            m_isUIHidden = !m_isUIHidden;
            m_viewport.SetUIHidden(m_isUIHidden);
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            if (!m_engine.IsProjectActive() || !m_engine.GetCurrentProject() || m_engine.GetCurrentProject()->GetSprites().empty()) return;
            m_isWizardMode = true;
            m_animBuilderPanel.Activate(m_engine);
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            if (m_engine.IsProjectActive() && m_engine.GetCurrentProject()) {
                PushUndoState(m_engine);

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
                
                project->SetSprites(newSprites);
                m_viewport.RefreshTexture(m_engine);
            }
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            auto selectedIds = m_viewport.GetSelectedSpriteIds();
            if (selectedIds.size() >= 2) {
                PushUndoState(m_engine);
                m_engine.MergeSelectedSprites(selectedIds);
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
            }
        },
        [this]() {
            m_isWandMode = false;
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            PushUndoState(m_engine);
            m_engine.CleanCurrentTexture();
            m_viewport.RefreshTexture(m_engine);
        },
        [this]() {
            m_isWandMode = !m_isWandMode;
            m_isWandEyedropper = false;
            m_isDraggingWandTol = false;
            m_isDraggingWandHue = false;
            m_isDraggingWandSatVal = false;
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
        },
        [this]() {
            m_isArtifactMode = !m_isArtifactMode;
            m_isWandMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
        },
        [this]() {
            m_isInfillMode = !m_isInfillMode;
            m_isWandMode = false;
            m_isArtifactMode = false;
            m_isDeleteMode = false;
        },
        [this]() {
            m_isDeleteMode = !m_isDeleteMode;
            m_isWandMode = false;
            m_isArtifactMode = false;
            m_isInfillMode = false;
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            PushUndoState(m_engine);
            m_engine.RepackFrames();
            m_viewport.RefreshTexture(m_engine);
        },
        [this]() {
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            PushUndoState(m_engine);
            m_engine.FlipHorizontal();
            m_viewport.RefreshTexture(m_engine);
        }
    );
    m_wandFont.loadFromFile("Resources/font.ttf");
    m_animationPanel->InitializeFont("Resources/font.ttf");
    m_exportPreview.InitializeFont("Resources/font.ttf");
    m_animBuilderPanel.InitializeFont("Resources/font.ttf");
    m_workspace.InitializeFont("Resources/font.ttf");
}

void SpriteSheetStudioPanel::LoadImage(const std::string& filePath) {
    std::string clean = CleanPath(filePath);
    std::string errorMsg;
    PushUndoState(m_engine);
    if (m_engine.ImportImage(clean, errorMsg)) {
        m_viewport.RefreshTexture(m_engine);
    }
}

void SpriteSheetStudioPanel::HandleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::FloatRect currentBounds(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    SetBounds(currentBounds);

    if (event.type == sf::Event::KeyPressed) {
        bool isShift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
        bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

        if (event.key.code == sf::Keyboard::Delete) {
            auto selectedIds = m_viewport.GetSelectedSpriteIds();
            if (!selectedIds.empty()) {
                PushUndoState(m_engine);
                for (const auto& id : selectedIds) {
                    m_engine.DeleteSpriteWithPixels(id);
                }
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
                return;
            }
        }

        if (isShift && event.key.code == sf::Keyboard::N) {
            if (m_engine.IsProjectActive()) {
                PushUndoState(m_engine);
                static int animCounter = 1;
                m_engine.CreateAnimation("New Animation " + std::to_string(animCounter++));
                m_viewport.RefreshTexture(m_engine);
                return;
            }
        }
        if (isControl) {
            if (event.key.code == sf::Keyboard::Z) {
                if (isShift) {
                    PerformRedo(m_engine);
                } else {
                    PerformUndo(m_engine);
                }
                m_viewport.ClearSelection();
                m_viewport.RefreshTexture(m_engine);
                return;
            }
            if (event.key.code == sf::Keyboard::Y) {
                PerformRedo(m_engine);
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

    if (m_toolbar.HandleEvent(event, window, m_engine)) return;

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        if (m_isWandMode) {
            float pw = 260.0f;
            float ph = 410.0f;
            float px0 = 18.0f;
            float py0 = 48.0f;
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
                    auto& rawPixels = const_cast<std::vector<uint8_t>&>(tex->GetPixels());
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

                    uint8_t targetR = rawPixels[targetIdx];
                    uint8_t targetG = rawPixels[targetIdx + 1];
                    uint8_t targetB = rawPixels[targetIdx + 2];
                    uint8_t targetA = rawPixels[targetIdx + 3];

                    if (targetA > 0) {
                        m_wandSelectionPixels.clear();
                        const float tolSq = m_wandTolerance * m_wandTolerance;

                        if (m_wandContiguous) {
                            std::vector<bool> visited(static_cast<size_t>(width * height), false);
                            std::queue<std::pair<int, int>> q;
                            q.push({px, py});
                            visited[py * width + px] = true;

                            const int dx[4] = {1, -1, 0, 0};
                            const int dy[4] = {0, 0, 1, -1};

                            while (!q.empty()) {
                                auto [cx, cy] = q.front();
                                q.pop();

                                m_wandSelectionPixels.push_back({cx, cy});

                                for (int d = 0; d < 4; ++d) {
                                    int nx = cx + dx[d];
                                    int ny = cy + dy[d];
                                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                        size_t nPos = static_cast<size_t>(ny * width + nx);
                                        if (!visited[nPos]) {
                                            visited[nPos] = true;
                                            size_t nIdx = nPos * 4;
                                            if (rawPixels[nIdx + 3] > 0) {
                                                float dr = static_cast<float>(rawPixels[nIdx]) - targetR;
                                                float dg = static_cast<float>(rawPixels[nIdx + 1]) - targetG;
                                                float db = static_cast<float>(rawPixels[nIdx + 2]) - targetB;
                                                if (dr * dr + dg * dg + db * db <= tolSq) {
                                                    q.push({nx, ny});
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            for (int y = 0; y < height; ++y) {
                                for (int x = 0; x < width; ++x) {
                                    size_t idx = static_cast<size_t>(y * width + x) * 4;
                                    if (rawPixels[idx + 3] > 0) {
                                        float dr = static_cast<float>(rawPixels[idx]) - targetR;
                                        float dg = static_cast<float>(rawPixels[idx + 1]) - targetG;
                                        float db = static_cast<float>(rawPixels[idx + 2]) - targetB;
                                        if (dr * dr + dg * dg + db * db <= tolSq) {
                                            m_wandSelectionPixels.push_back({x, y});
                                        }
                                    }
                                }
                            }
                        }
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
                    auto& rawPixels = const_cast<std::vector<uint8_t>&>(tex->GetPixels());
                    size_t targetIdx = static_cast<size_t>(py * width + px) * 4;

                    uint8_t targetR = rawPixels[targetIdx];
                    uint8_t targetG = rawPixels[targetIdx + 1];
                    uint8_t targetB = rawPixels[targetIdx + 2];
                    uint8_t targetA = rawPixels[targetIdx + 3];

                    if (targetA > 0) {
                        PushUndoState(m_engine);

                        if (m_isDeleteMode) {
                            std::vector<bool> visited(static_cast<size_t>(width * height), false);
                            std::queue<std::pair<int, int>> q;
                            q.push({px, py});
                            visited[py * width + px] = true;

                            const int dx[4] = {1, -1, 0, 0};
                            const int dy[4] = {0, 0, 1, -1};

                            while (!q.empty()) {
                                auto [cx, cy] = q.front();
                                q.pop();

                                size_t cIdx = static_cast<size_t>(cy * width + cx) * 4;
                                rawPixels[cIdx] = 0;
                                rawPixels[cIdx + 1] = 0;
                                rawPixels[cIdx + 2] = 0;
                                rawPixels[cIdx + 3] = 0;

                                for (int d = 0; d < 4; ++d) {
                                    int nx = cx + dx[d];
                                    int ny = cy + dy[d];
                                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                        size_t nPos = static_cast<size_t>(ny * width + nx);
                                        if (!visited[nPos]) {
                                            visited[nPos] = true;
                                            size_t nIdx = nPos * 4;
                                            if (rawPixels[nIdx + 3] > 0) {
                                                int dr = std::abs(static_cast<int>(rawPixels[nIdx]) - targetR);
                                                int dg = std::abs(static_cast<int>(rawPixels[nIdx + 1]) - targetG);
                                                int db = std::abs(static_cast<int>(rawPixels[nIdx + 2]) - targetB);
                                                if (dr <= 25 && dg <= 25 && db <= 25) {
                                                    q.push({nx, ny});
                                                }
                                            }
                                        }
                                    }
                                }
                            }
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
            float py0 = 48.0f;
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

            PushUndoState(m_engine);

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
                    PushUndoState(m_engine);
                    for (const auto& id : targetsToDelete) {
                        m_engine.DeleteSpriteWithPixels(id);
                    }
                    m_viewport.ClearSelection();
                    m_viewport.RefreshTexture(m_engine);
                }},
                {pivotLabel, [this, targetsToDelete]() {
                    auto proj = m_engine.GetCurrentProject();
                    if (!proj) return;
                    PushUndoState(m_engine);
                    for (const auto& id : targetsToDelete) {
                        auto sprite = proj->GetSpriteById(id);
                        if (sprite) {
                            auto rect = sprite->GetSourceRect();
                            sprite->SetPivot({rect.width / 2.0f, rect.height / 2.0f});
                        }
                    }
                    m_viewport.RefreshTexture(m_engine);
                }}
            };
            m_workspace.ShowContextMenu({static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y)}, items);
            return;
        }
    }

    if (m_animationPanel) {
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
    if (!m_isWizardMode) {
        m_engine.Update(deltaTime);
        m_viewport.Update(deltaTime);
        m_wandAntsOffset += deltaTime * 24.0f;
        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
        int totalSprites = (m_engine.IsProjectActive() && m_engine.GetCurrentProject())
                            ? static_cast<int>(m_engine.GetCurrentProject()->GetSprites().size()) : 0;
        int selectedCount = static_cast<int>(m_viewport.GetSelectedSpriteIds().size());
        m_workspace.UpdateStatusBar(m_viewport.GetZoom(), worldPos, totalSprites, selectedCount, "Ready");
    }
}

void SpriteSheetStudioPanel::SetBounds(const sf::FloatRect& bounds) {
    m_bounds = bounds;
    m_toolbar.SetBounds(bounds);
    m_viewport.SetBounds(bounds);
    m_workspace.SetBounds(bounds);
    
    if (m_animationPanel) {
        m_animationPanel->SetBounds(bounds);
    }
}

void SpriteSheetStudioPanel::Render(sf::RenderWindow& window) {
    if (!m_isActive) return;

    sf::FloatRect currentBounds(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    SetBounds(currentBounds);

    sf::View uiView(currentBounds);
    window.setView(uiView);

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

        if (m_isWandMode) {
            float pw = 240.0f;
            float ph = 312.0f;
            float px0 = m_bounds.width - pw - 14.0f;
            float py0 = 48.0f;

            sf::RectangleShape pbg({pw, ph});
            pbg.setPosition(px0, py0);
            pbg.setFillColor(sf::Color(22, 18, 30, 245));
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

            sf::RectangleShape sliderTrack({pw - 88.0f, 8.0f});
            sliderTrack.setPosition(px0 + 44.0f, py0 + 58.0f);
            sliderTrack.setFillColor(sf::Color(12, 10, 16));
            window.draw(sliderTrack);

            float fillFrac = std::clamp(m_wandTolerance / 128.0f, 0.0f, 1.0f);
            sf::RectangleShape sliderFill({(pw - 88.0f) * fillFrac, 8.0f});
            sliderFill.setPosition(px0 + 44.0f, py0 + 58.0f);
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

            sf::RectangleShape tabDel({104.0f, 24.0f});
            tabDel.setPosition(px0 + 12.0f, py0 + 82.0f);
            tabDel.setFillColor(m_wandActionDelete ? sf::Color(190, 45, 45) : Theme::PanelBackground);
            tabDel.setOutlineThickness(1.0f);
            tabDel.setOutlineColor(m_wandActionDelete ? sf::Color(255, 80, 80) : Theme::BorderColor);
            window.draw(tabDel);
            sf::Text delText(u8"Delete", m_wandFont, 11);
            delText.setFillColor(sf::Color::White);
            delText.setPosition(px0 + 44.0f, py0 + 86.0f);
            window.draw(delText);

            sf::RectangleShape tabRec({104.0f, 24.0f});
            tabRec.setPosition(px0 + 124.0f, py0 + 82.0f);
            tabRec.setFillColor(!m_wandActionDelete ? Theme::AccentColor : Theme::PanelBackground);
            tabRec.setOutlineThickness(1.0f);
            tabRec.setOutlineColor(!m_wandActionDelete ? Theme::AccentHoverColor : Theme::BorderColor);
            window.draw(tabRec);
            sf::Text recText(u8"Recolor", m_wandFont, 11);
            recText.setFillColor(sf::Color::White);
            recText.setPosition(px0 + 154.0f, py0 + 86.0f);
            window.draw(recText);

            const sf::Color swatches[12] = {
                sf::Color(230, 45, 45), sf::Color(245, 130, 35), sf::Color(255, 215, 0),
                sf::Color(55, 205, 95), sf::Color(35, 175, 245), sf::Color(55, 85, 245),
                sf::Color(165, 55, 245), sf::Color(245, 55, 175), sf::Color(255, 255, 255),
                sf::Color(170, 170, 170), sf::Color(65, 65, 65), sf::Color(10, 10, 10)
            };
            for (int i = 0; i < 12; ++i) {
                float sx = px0 + 12.0f + (i % 6) * 36.0f;
                float sy = py0 + 114.0f + (i / 6) * 30.0f;
                sf::RectangleShape swatch({30.0f, 24.0f});
                swatch.setPosition(sx, sy);
                swatch.setFillColor(swatches[i]);
                bool isCur = (!m_wandActionDelete && swatches[i] == m_wandSelectedColor);
                swatch.setOutlineThickness(isCur ? 2.0f : 1.0f);
                swatch.setOutlineColor(isCur ? sf::Color::White : Theme::BorderColor);
                window.draw(swatch);
            }

            sf::RectangleShape eyeBtn({pw - 24.0f, 24.0f});
            eyeBtn.setPosition(px0 + 12.0f, py0 + 180.0f);
            eyeBtn.setFillColor(m_isWandEyedropper ? sf::Color(45, 140, 75) : Theme::PanelBackground);
            eyeBtn.setOutlineThickness(1.0f);
            eyeBtn.setOutlineColor(m_isWandEyedropper ? sf::Color(80, 220, 120) : Theme::BorderColor);
            window.draw(eyeBtn);

            sf::RectangleShape activeColorBox({14.0f, 14.0f});
            activeColorBox.setPosition(px0 + 18.0f, py0 + 185.0f);
            activeColorBox.setFillColor(m_wandSelectedColor);
            activeColorBox.setOutlineThickness(1.0f);
            activeColorBox.setOutlineColor(sf::Color::White);
            window.draw(activeColorBox);

            sf::Text eyeText(m_isWandEyedropper ? u8"Click Canvas to Sample" : u8"Eyedrop Color", m_wandFont, 11);
            eyeText.setFillColor(sf::Color::White);
            eyeText.setPosition(px0 + 42.0f, py0 + 184.0f);
            window.draw(eyeText);

            sf::RectangleShape applyBtn({pw - 24.0f, 28.0f});
            applyBtn.setPosition(px0 + 12.0f, py0 + 214.0f);
            applyBtn.setFillColor(m_wandActionDelete ? sf::Color(180, 40, 40) : Theme::AccentColor);
            applyBtn.setOutlineThickness(1.0f);
            applyBtn.setOutlineColor(Theme::AccentHoverColor);
            window.draw(applyBtn);

            std::string applyStr = m_wandActionDelete ? u8"Delete Selection" : u8"Recolor Selection";
            sf::Text aText(applyStr, m_wandFont, 11);
            aText.setFillColor(sf::Color::White);
            aText.setPosition(px0 + 36.0f, py0 + 220.0f);
            window.draw(aText);

            sf::RectangleShape clearBtn({pw - 24.0f, 22.0f});
            clearBtn.setPosition(px0 + 12.0f, py0 + 248.0f);
            clearBtn.setFillColor(Theme::PanelBackground);
            clearBtn.setOutlineThickness(1.0f);
            clearBtn.setOutlineColor(Theme::BorderColor);
            window.draw(clearBtn);

            sf::Text cText(u8"Deselect All", m_wandFont, 10);
            cText.setFillColor(Theme::TextMuted);
            cText.setPosition(px0 + 82.0f, py0 + 252.0f);
            window.draw(cText);

            std::string countStr = std::to_string(m_wandSelectionPixels.size()) + u8" pixels selected";
            sf::Text countText(countStr, m_wandFont, 10);
            countText.setFillColor(Theme::TextMuted);
            countText.setPosition(px0 + 14.0f, py0 + 280.0f);
            window.draw(countText);
        }

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

        if (!m_isUIHidden && m_animationPanel) {
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
            float py0 = 48.0f;

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

    PushUndoState(m_engine);

    auto tex = m_engine.GetCurrentTexture();
    int width = tex->GetWidth();
    auto& rawPixels = const_cast<std::vector<uint8_t>&>(tex->GetPixels());

    for (const auto& pt : m_wandSelectionPixels) {
        size_t idx = static_cast<size_t>(pt.y * width + pt.x) * 4;
        if (m_wandActionDelete) {
            rawPixels[idx] = 0;
            rawPixels[idx + 1] = 0;
            rawPixels[idx + 2] = 0;
            rawPixels[idx + 3] = 0;
        } else {
            rawPixels[idx] = m_wandSelectedColor.r;
            rawPixels[idx + 1] = m_wandSelectedColor.g;
            rawPixels[idx + 2] = m_wandSelectedColor.b;
            rawPixels[idx + 3] = m_wandSelectedColor.a;
        }
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