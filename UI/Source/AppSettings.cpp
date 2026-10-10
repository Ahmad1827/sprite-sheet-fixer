#include "AppSettings.h"
#include "json.hpp"
#include <fstream>

namespace StudioUI {

using json = nlohmann::json;

AppSettings AppSettings::Load(const std::string& path) {
    AppSettings settings;
    std::ifstream file(path);
    if (!file.is_open()) return settings;

    try {
        json j;
        file >> j;
        settings.cleanSoftEdges = j.value("cleanSoftEdges", settings.cleanSoftEdges);
        settings.cleanRemoveFrameLines = j.value("cleanRemoveFrameLines", settings.cleanRemoveFrameLines);
        settings.cleanSpeckSize = j.value("cleanSpeckSize", settings.cleanSpeckSize);
        settings.aiEnabled = j.value("aiEnabled", settings.aiEnabled);
        settings.aiCommand = j.value("aiCommand", settings.aiCommand);
        settings.aiModel = j.value("aiModel", settings.aiModel);
        settings.aiRemoveLabels = j.value("aiRemoveLabels", settings.aiRemoveLabels);
        settings.aiRemoveDebris = j.value("aiRemoveDebris", settings.aiRemoveDebris);
        settings.aiAskFirst = j.value("aiAskFirst", settings.aiAskFirst);
        settings.aiExtraInstructions = j.value("aiExtraInstructions", settings.aiExtraInstructions);
    } catch (const json::exception&) {
        // A damaged file just means starting again from the defaults.
        return AppSettings{};
    }
    if (settings.aiCommand.empty()) settings.aiCommand = "claude";
    return settings;
}

bool AppSettings::Save(const std::string& path) const {
    json j;
    j["cleanSoftEdges"] = cleanSoftEdges;
    j["cleanRemoveFrameLines"] = cleanRemoveFrameLines;
    j["cleanSpeckSize"] = cleanSpeckSize;
    j["aiEnabled"] = aiEnabled;
    j["aiCommand"] = aiCommand;
    j["aiModel"] = aiModel;
    j["aiRemoveLabels"] = aiRemoveLabels;
    j["aiRemoveDebris"] = aiRemoveDebris;
    j["aiAskFirst"] = aiAskFirst;
    j["aiExtraInstructions"] = aiExtraInstructions;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(4);
    return true;
}

}
