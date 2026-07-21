#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/AnimationGroup.h"

namespace StudioCore {

void Project::SetTexture(std::shared_ptr<const SourceTexture> texture) {
    m_texture = std::move(texture);
}

std::shared_ptr<const SourceTexture> Project::GetTexture() const {
    return m_texture;
}

void Project::AddSprite(std::shared_ptr<SpriteDefinition> sprite) {
    if (sprite) {
        m_sprites.push_back(std::move(sprite));
    }
}

const std::vector<std::shared_ptr<SpriteDefinition>>& Project::GetSprites() const {
    return m_sprites;
}

void Project::AddAnimationGroup(std::shared_ptr<AnimationGroup> group) {
    if (group) {
        m_animations.push_back(std::move(group));
    }
}

const std::vector<std::shared_ptr<AnimationGroup>>& Project::GetAnimationGroups() const {
    return m_animations;
}

ExportSettings& Project::GetExportSettings() {
    return m_exportSettings;
}

const ExportSettings& Project::GetExportSettings() const {
    return m_exportSettings;
}

}