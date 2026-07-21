#pragma once
#include <string>

namespace StudioCore {

struct Rect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

struct Point {
    float x{0.0f};
    float y{0.0f};
};

class SpriteDefinition {
public:
    SpriteDefinition(std::string id, const Rect& sourceRect);

    const std::string& GetId() const;
    const Rect& GetSourceRect() const;
    
    void SetPivot(const Point& pivot);
    const Point& GetPivot() const;

    void SetBaseline(float baseline);
    float GetBaseline() const;

private:
    std::string m_id;
    Rect m_sourceRect;
    Point m_pivot{0.5f, 0.5f};
    float m_baseline{1.0f};
};

}