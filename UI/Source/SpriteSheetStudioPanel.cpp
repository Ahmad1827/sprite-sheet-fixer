#include "SpriteSheetStudioPanel.h"
#include "Utils/NativeFileDialog.h"
#include "DataModels/Project.h"
#include "Processing/ImageLoader.h"
#include "DataModels/SpriteDefinition.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <fstream>
#include <iostream>
#include <filesystem>

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

static std::vector<std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>> g_undoSprites;
static std::vector<std::vector<std::shared_ptr<StudioCore::SpriteDefinition>>> g_redoSprites;

static void PushUndoState(StudioCore::StudioEngineFacade& engine) {
    if (!engine.IsProjectActive() || !engine.GetCurrentProject()) return;
    auto project = engine.GetCurrentProject();
    std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> cloned;
    for (const auto& s : project->GetSprites()) {
        if (!s) continue;
        auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
        copy->SetPivot(s->GetPivot());
        cloned.push_back(copy);
    }
    g_undoSprites.push_back(cloned);
    g_redoSprites.clear();
}

static void PerformUndo(StudioCore::StudioEngineFacade& engine) {
    if (!g_undoSprites.empty() && engine.IsProjectActive() && engine.GetCurrentProject()) {
        auto project = engine.GetCurrentProject();
        std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> currentCloned;
        for (const auto& s : project->GetSprites()) {
            if (!s) continue;
            auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
            copy->SetPivot(s->GetPivot());
            currentCloned.push_back(copy);
        }
        g_redoSprites.push_back(currentCloned);

        auto prevState = g_undoSprites.back();
        g_undoSprites.pop_back();

        project->SetSprites(prevState);
    }
    engine.Undo();
}

static void PerformRedo(StudioCore::StudioEngineFacade& engine) {
    if (!g_redoSprites.empty() && engine.IsProjectActive() && engine.GetCurrentProject()) {
        auto project = engine.GetCurrentProject();
        std::vector<std::shared_ptr<StudioCore::SpriteDefinition>> currentCloned;
        for (const auto& s : project->GetSprites()) {
            if (!s) continue;
            auto copy = std::make_shared<StudioCore::SpriteDefinition>(s->GetId(), s->GetSourceRect());
            copy->SetPivot(s->GetPivot());
            currentCloned.push_back(copy);
        }
        g_undoSprites.push_back(currentCloned);

        auto nextState = g_redoSprites.back();
        g_redoSprites.pop_back();

        project->SetSprites(nextState);
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

    std::cout << "[SpriteSheetStudio] Atlas generated: " << atlasJsonPath << " (" << sprites.size() << " sprites)" << std::endl;
    std::cout << "[SpriteSheetStudio] Atlas generated: " << atlasTxtPath << std::endl;
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
                            bestIdx = i;
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
            m_isArtifactMode = false;
            m_isInfillMode = false;
            m_isDeleteMode = false;
            PushUndoState(m_engine);
            m_engine.CleanCurrentTexture();
            m_viewport.RefreshTexture(m_engine);
        },
        [this]() {
            m_isArtifactMode = !m_isArtifactMode;
            m_isInfillMode = false;
            m_isDeleteMode = false;
        },
        [this]() {
            m_isInfillMode = !m_isInfillMode;
            m_isArtifactMode = false;
            m_isDeleteMode = false;
        },
        [this]() {
            m_isDeleteMode = !m_isDeleteMode;
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
        if (event.key.code == sf::Keyboard::A) {
            m_engine.ToggleAutoAlign();
            m_viewport.RefreshTexture(m_engine);
            return;
        }
        if (event.key.code == sf::Keyboard::Escape) {
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
        if (m_isDraggingArtifact) {
            sf::Vector2i pixelPos(event.mouseMove.x, event.mouseMove.y);
            m_artifactDragCurrent = m_viewport.MapPixelToWorld(pixelPos, window);
            m_dragCurrentPixel = pixelPos;
            return;
        }
    }

    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
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
            std::vector<StudioUI::ContextMenuItem> items = {
                {"Delete Sprite", [this, targetSpriteId]() {
                    PushUndoState(m_engine);
                    m_engine.DeleteSpriteWithPixels(targetSpriteId);
                    m_viewport.RefreshTexture(m_engine);
                }},
                {"Reset Pivot", [this, targetSpriteId]() {
                    auto proj = m_engine.GetCurrentProject();
                    if (!proj) return;
                    auto sprite = proj->GetSpriteById(targetSpriteId);
                    if (sprite) {
                        PushUndoState(m_engine);
                        auto rect = sprite->GetSourceRect();
                        sprite->SetPivot({rect.width / 2.0f, rect.height / 2.0f});
                        m_viewport.RefreshTexture(m_engine);
                    }
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

        if (!m_isUIHidden && m_animationPanel) {
            m_animationPanel->Render(window, m_engine);
        }
        m_toolbar.Render(window);
        if (m_isWizardMode) {
            m_animBuilderPanel.Render(window);
        }
        m_workspace.Render(window);
    }
}

}