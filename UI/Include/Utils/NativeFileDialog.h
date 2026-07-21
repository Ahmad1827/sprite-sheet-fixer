#pragma once
#include <string>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace StudioUI {

class NativeFileDialog {
public:
    static std::string OpenFileDialog(const std::string& filterName = "PNG Image (*.png)") {
#ifdef _WIN32
        char szFile[260] = {0};

        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            return std::string(ofn.lpstrFile);
        }
        return "";
#else
        std::string command = "zenity --file-selection --file-filter=\"PNG Image | *.png\" 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        return result;
#endif
    }

    static std::string SaveFileDialog(const std::string& defaultName = "sprite_sheet.png") {
#ifdef _WIN32
        char szFile[260] = {0};
        strncpy(szFile, defaultName.c_str(), sizeof(szFile) - 1);

        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "PNG Image (*.png)\0*.png\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

        if (GetSaveFileNameA(&ofn) == TRUE) {
            std::string path = ofn.lpstrFile;
            if (path.find(".png") == std::string::npos) {
                path += ".png";
            }
            return path;
        }
        return "";
#else
        std::string command = "zenity --file-selection --save --confirm-overwrite --filename=\"" + defaultName + "\" --file-filter=\"PNG Image | *.png\" 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }

        if (!result.empty() && result.find(".png") == std::string::npos) {
            result += ".png";
        }
        return result;
#endif
    }
};

}