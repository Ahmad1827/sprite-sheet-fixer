#pragma once
#include <string>

namespace StudioUI {

// Everything the user can choose in the Settings window. Saved next to the
// program as settings.json.
struct AppSettings {
    // Clean
    bool cleanSoftEdges{true};
    bool cleanRemoveFrameLines{true};
    int cleanSpeckSize{0};              // largest leftover (in pixels) Clean also deletes; 0 = keep all

    // AI review through Claude Code
    bool aiEnabled{true};
    std::string aiCommand{"claude"};    // how to start Claude Code on this computer
    std::string aiModel{"default"};     // default, haiku, sonnet or opus
    bool aiRemoveLabels{true};
    bool aiRemoveDebris{true};
    bool aiAskFirst{true};              // show what would be deleted and wait for a yes
    std::string aiExtraInstructions;

    static AppSettings Load(const std::string& path = "settings.json");
    bool Save(const std::string& path = "settings.json") const;
};

}
