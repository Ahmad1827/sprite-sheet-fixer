#include "Systems/ProjectManager.h"
#include "DataModels/Project.h"
#include "DataModels/AnimationGroup.h"
#include "Processing/ImageLoader.h"
#include "json.hpp"
#include <fstream>
#include <set>

using json = nlohmann::json;

namespace StudioCore {

bool ProjectManager::SaveProject(const Project& project, const std::string& filePath) {
    json j;
    j["imagePath"] = project.GetImagePath();
    
    json spritesArray = json::array();
    for (const auto& s : project.GetSprites()) {
        const auto& rect = s->GetSourceRect();
        const auto& pivot = s->GetPivot();
        spritesArray.push_back({
            {"id", s->GetId()},
            {"x", rect.x},
            {"y", rect.y},
            {"width", rect.width},
            {"height", rect.height},
            {"pivotX", pivot.x},
            {"pivotY", pivot.y},
            {"baseline", s->GetBaseline()}
        });
    }
    j["sprites"] = spritesArray;

    json animsArray = json::array();
    for (const auto& a : project.GetAnimationGroups()) {
        animsArray.push_back({
            {"id", a->GetId()},
            {"name", a->GetName()},
            {"fps", a->GetFPS()},
            {"looping", a->IsLooping()},
            {"frames", a->GetFrames()}
        });
    }
    j["animations"] = animsArray;

    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << j.dump(4);
    return true;
}

std::shared_ptr<Project> ProjectManager::LoadProject(const std::string& filePath, std::string& outError) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        outError = "Could not open project file.";
        return nullptr;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        outError = "Corrupted JSON project file.";
        return nullptr;
    }

    std::string imgPath = j.value("imagePath", "");
    std::ifstream imgCheck(imgPath);
    if (!imgCheck.good()) {
        outError = "Missing source image: " + imgPath;
        return nullptr;
    }

    auto project = std::make_shared<Project>();
    auto texture = ImageLoader::LoadFromFile(imgPath, outError);
    if (!texture) return nullptr;
    project->SetTexture(std::move(texture));
    project->SetImagePath(imgPath);

    std::set<std::string> seenIds;
    if (j.contains("sprites")) {
        for (const auto& s : j["sprites"]) {
            std::string id = s["id"];
            if (seenIds.find(id) != seenIds.end()) {
                outError = "Duplicate sprite ID detected: " + id;
                return nullptr;
            }
            seenIds.insert(id);

            Rect rect{s["x"], s["y"], s["width"], s["height"]};
            SpriteDefinition def(id, rect);
            def.SetPivot({s["pivotX"], s["pivotY"]});
            def.SetBaseline(s["baseline"]);
            project->AddSprite(def);
        }
    }

    if (j.contains("animations")) {
        for (const auto& a : j["animations"]) {
            auto anim = std::make_shared<AnimationGroup>(a["id"], a["name"]);
            anim->SetFPS(a["fps"]);
            anim->SetLooping(a["looping"]);
            std::vector<std::string> frames = a["frames"];
            anim->SetFrames(frames);
            project->AddAnimationGroup(anim);
        }
    }

    return project;
}

}