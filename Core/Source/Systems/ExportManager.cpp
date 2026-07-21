#include "Systems/ExportManager.h"
#include "Processing/AtlasPacker.h"
#include "DataModels/Project.h"
#include "DataModels/AnimationGroup.h"
#include "json.hpp"
#include <fstream>

using json = nlohmann::json;

namespace StudioCore {

sf::Image ExportManager::GeneratePreview(const Project& project, int padding) const {
    return AtlasPacker::PackUniformGrid(project, padding).image;
}

bool ExportManager::ExportPNG(const Project& project, const std::string& filePath, int padding) const {
    AtlasResult result = AtlasPacker::PackUniformGrid(project, padding);
    if (result.image.getSize().x == 0 || result.image.getSize().y == 0) return false;
    if (!result.image.saveToFile(filePath)) return false;

    std::string baseDir = filePath;
    size_t lastSlash = baseDir.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDir = baseDir.substr(0, lastSlash + 1);
    } else {
        baseDir = "";
    }

    json atlasJson = json::array();
    for (const auto& s : result.sprites) {
        atlasJson.push_back({
            {"id", s.id},
            {"name", s.name},
            {"x", s.x},
            {"y", s.y},
            {"width", s.width},
            {"height", s.height},
            {"pivot", {{"x", s.pivotX}, {"y", s.pivotY}}},
            {"baseline", s.baseline}
        });
    }
    std::ofstream atlasFile(baseDir + "atlas.json");
    atlasFile << atlasJson.dump(4);

    json animJson = json::array();
    for (const auto& a : project.GetAnimationGroups()) {
        animJson.push_back({
            {"name", a->GetName()},
            {"fps", a->GetFPS()},
            {"looping", a->IsLooping()},
            {"frames", a->GetFrames()}
        });
    }
    std::ofstream animFile(baseDir + "animations.json");
    animFile << animJson.dump(4);

    return true;
}

}