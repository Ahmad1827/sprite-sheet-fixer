#include <SFML/Graphics.hpp>
#include "SpriteSheetStudioPanel.h"
#include <vector>
#include <cstdint>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <queue>
#include <mutex>
#include <string>

#ifdef LoadImage
#undef LoadImage
#endif

#ifndef WM_COPYGLOBALDATA
#define WM_COPYGLOBALDATA 0x0049
#endif

#ifndef MSGFLT_ALLOW
#define MSGFLT_ALLOW 1
#endif

static std::queue<std::string> g_droppedFiles;
static std::mutex g_dropMutex;
static WNDPROC g_originalWndProc = nullptr;

static LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DROPFILES) {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);
        
        for (UINT i = 0; i < fileCount; ++i) {
            char filePath[MAX_PATH];
            if (DragQueryFileA(hDrop, i, filePath, MAX_PATH)) {
                std::lock_guard<std::mutex> lock(g_dropMutex);
                g_droppedFiles.push(std::string(filePath));
            }
        }
        
        DragFinish(hDrop);
        return 0;
    }
    return CallWindowProc(g_originalWndProc, hwnd, msg, wParam, lParam);
}

typedef BOOL(WINAPI* PFN_ChangeWindowMessageFilterEx)(HWND, UINT, DWORD, PVOID);

static void EnableDropPrivileges(HWND hwnd) {
    DragAcceptFiles(hwnd, TRUE);

    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        PFN_ChangeWindowMessageFilterEx pChangeWindowMessageFilterEx =
            reinterpret_cast<PFN_ChangeWindowMessageFilterEx>(GetProcAddress(hUser32, "ChangeWindowMessageFilterEx"));
        
        if (pChangeWindowMessageFilterEx) {
            pChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
            pChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
            pChangeWindowMessageFilterEx(hwnd, WM_COPYGLOBALDATA, MSGFLT_ALLOW, nullptr);
        }
    }
}
#endif

static sf::Image DownscaleIcon(const sf::Image& src, unsigned int targetSize = 32) {
    sf::Image dest;
    dest.create(targetSize, targetSize);
    unsigned int srcW = src.getSize().x;
    unsigned int srcH = src.getSize().y;

    for (unsigned int y = 0; y < targetSize; ++y) {
        for (unsigned int x = 0; x < targetSize; ++x) {
            unsigned int srcX = (x * srcW) / targetSize;
            unsigned int srcY = (y * srcH) / targetSize;
            dest.setPixel(x, y, src.getPixel(srcX, srcY));
        }
    }
    return dest;
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Sprite Sheet Fixer");
    window.setFramerateLimit(60);

    sf::Image rawIcon;
    bool loaded = rawIcon.loadFromFile("Resources/icon.png") ||
                  rawIcon.loadFromFile("testiconssffixed.jpg") ||
                  rawIcon.loadFromFile("Resources/icon.jpg");

    if (loaded) {
        sf::Image safeIcon = DownscaleIcon(rawIcon, 32);
        window.setIcon(safeIcon.getSize().x, safeIcon.getSize().y, safeIcon.getPixelsPtr());
    } else {
        std::vector<uint8_t> pixels(16 * 16 * 4, 0);
        const char* keyMap[16] = {
            "................",
            ".....#####......",
            "....#OOOOO#.....",
            "....#O...O#.....",
            "....#O...O#.....",
            "....#OOOOO#.....",
            ".....##O##......",
            "......#O#.......",
            "......#O#.......",
            "......#O#.......",
            "......#O##......",
            "......#OOO#.....",
            "......#O##......",
            "......#OOO#.....",
            "......#O#.......",
            ".......#........"
        };
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                int idx = (y * 16 + x) * 4;
                char c = keyMap[y][x];
                if (c == '#') {
                    pixels[idx + 0] = 160;
                    pixels[idx + 1] = 110;
                    pixels[idx + 2] = 20;
                    pixels[idx + 3] = 255;
                } else if (c == 'O') {
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 210;
                    pixels[idx + 2] = 40;
                    pixels[idx + 3] = 255;
                }
            }
        }
        rawIcon.create(16, 16, pixels.data());
        window.setIcon(16, 16, rawIcon.getPixelsPtr());
    }

#if defined(_WIN32)
    HWND hwnd = window.getSystemHandle();
    EnableDropPrivileges(hwnd);
    g_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CustomWndProc)));
#endif

    StudioUI::SpriteSheetStudioPanel editorPanel;
    editorPanel.Initialize();

    sf::Clock clock;

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
                editorPanel.SetBounds(visibleArea);
            }

            editorPanel.HandleEvent(event, window);
        }

#if defined(_WIN32)
        {
            std::lock_guard<std::mutex> lock(g_dropMutex);
            while (!g_droppedFiles.empty()) {
                editorPanel.LoadImage(g_droppedFiles.front());
                g_droppedFiles.pop();
            }
        }
#endif

        editorPanel.Update(deltaTime, window);

        window.clear();
        editorPanel.Render(window);
        window.display();
    }

    return 0;
}