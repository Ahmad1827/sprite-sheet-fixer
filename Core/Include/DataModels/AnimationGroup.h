#pragma once
#include <string>
#include <vector>
#include <memory>

namespace StudioCore {

class SpriteDefinition;

class AnimationGroup {
public:
    explicit AnimationGroup(std::string name);

    const std::string& GetName() const;
    
    void SetFps(int fps);
    int GetFps() const;

    void SetLooping(bool looping);
    bool IsLooping() const;

    void AddFrame(std::shared_ptr<SpriteDefinition> sprite);
    void RemoveFrame(size_t index);
    const std::vector<std::shared_ptr<SpriteDefinition>>& GetFrames() const;

private:
    std::string m_name;
    int m_fps{12};
    bool m_looping{true};
    std::vector<std::shared_ptr<SpriteDefinition>> m_frames;
};

}