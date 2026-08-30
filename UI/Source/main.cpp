#include <SFML/Graphics.hpp>
#include "SpriteSheetStudioPanel.h"

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

int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Sprite Sheet Fixer");
    window.setFramerateLimit(60);

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