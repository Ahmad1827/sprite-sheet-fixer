#pragma once
#include <vector>
#include <memory>
#include <string>
#include "ExportSettings.h"

namespace StudioCore {

class SourceTexture;
class SpriteDefinition;
class AnimationGroup;

class Project {
public:
    Project() = default;

    void SetTexture(std::shared_ptr<const SourceTexture> texture);
    std::shared_ptr<const SourceTexture> GetTexture() const;

    void AddSprite(std::shared_ptr<SpriteDefinition> sprite);
    const std::vector<std::shared_ptr<SpriteDefinition>>& GetSprites() const;
    std::shared_ptr<SpriteDefinition> GetSpriteById(const std::string& id) const;

    void AddAnimationGroup(std::shared_ptr<AnimationGroup> group);
    void RemoveAnimationGroup(const std::string& id);
    const std::vector<std::shared_ptr<AnimationGroup>>& GetAnimationGroups() const;
    std::shared_ptr<AnimationGroup> GetAnimationById(const std::string& id) const;

    ExportSettings& GetExportSettings();
    const ExportSettings& GetExportSettings() const;

private:
    std::shared_ptr<const SourceTexture> m_texture;
    std::vector<std::shared_ptr<SpriteDefinition>> m_sprites;
    std::vector<std::shared_ptr<AnimationGroup>> m_animations;
    ExportSettings m_exportSettings;
};

}