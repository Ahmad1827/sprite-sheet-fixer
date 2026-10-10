#pragma once
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace StudioUI {

class ProcessRunner {
public:
    // Runs a command line with its input, output and errors connected to files
    // and waits for it to finish. Returns the exit code, or -1 if it could not
    // be started. No console window is shown on Windows.
    static int Run(const std::string& command, const std::string& inputPath,
                   const std::string& outputPath, const std::string& errorPath) {
        std::string full = command;
        if (!inputPath.empty()) full += " < \"" + inputPath + "\"";
        full += " > \"" + outputPath + "\" 2> \"" + errorPath + "\"";

#ifdef _WIN32
        // cmd strips one pair of outer quotes, so the whole line gets its own.
        std::string cmdLine = "cmd.exe /S /C \"" + full + "\"";
        STARTUPINFOA startup;
        PROCESS_INFORMATION process;
        ZeroMemory(&startup, sizeof(startup));
        ZeroMemory(&process, sizeof(process));
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;

        std::string mutableLine = cmdLine;
        if (!CreateProcessA(nullptr, mutableLine.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
            return -1;
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
        return static_cast<int>(exitCode);
#else
        int status = std::system(full.c_str());
        if (status == -1) return -1;
        return (status >> 8) & 0xff;
#endif
    }

    static std::string ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

}
