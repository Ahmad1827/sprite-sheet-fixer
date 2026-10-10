#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

namespace StudioUI {

inline void DrawRoundedRect(sf::RenderTarget& target, const sf::FloatRect& rect, float radius,
                            sf::Color fill, sf::Color outline = sf::Color::Transparent, float outlineThickness = 0.0f) {
    radius = std::max(0.0f, std::min({ radius, rect.width / 2.0f, rect.height / 2.0f }));
    const int steps = 5;
    sf::ConvexShape shape((steps + 1) * 4);

    const sf::Vector2f centers[4] = {
        { rect.left + rect.width - radius, rect.top + radius },
        { rect.left + rect.width - radius, rect.top + rect.height - radius },
        { rect.left + radius, rect.top + rect.height - radius },
        { rect.left + radius, rect.top + radius }
    };
    const float startAngles[4] = { -90.0f, 0.0f, 90.0f, 180.0f };

    int point = 0;
    for (int corner = 0; corner < 4; ++corner) {
        for (int s = 0; s <= steps; ++s) {
            float angle = (startAngles[corner] + 90.0f * s / steps) * 3.14159265f / 180.0f;
            shape.setPoint(point++, { centers[corner].x + std::cos(angle) * radius, centers[corner].y + std::sin(angle) * radius });
        }
    }

    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outlineThickness);
    target.draw(shape);
}

inline sf::Color MixColor(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return sf::Color(
        static_cast<sf::Uint8>(a.r + (b.r - a.r) * t),
        static_cast<sf::Uint8>(a.g + (b.g - a.g) * t),
        static_cast<sf::Uint8>(a.b + (b.b - a.b) * t),
        static_cast<sf::Uint8>(a.a + (b.a - a.a) * t));
}

}
