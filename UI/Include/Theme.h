#pragma once
#include <SFML/Graphics/Color.hpp>

namespace StudioUI {

namespace Theme {

inline const sf::Color MainBackground{18, 14, 24};
inline const sf::Color PanelBackground{28, 22, 40};
inline const sf::Color InspectorBackground{24, 18, 34};
inline const sf::Color ViewportBackground{14, 11, 20};
inline const sf::Color BorderColor{75, 48, 110};
inline const sf::Color HoverColor{55, 36, 85};
inline const sf::Color ActiveColor{140, 80, 255};

inline const sf::Color BevelLight{90, 60, 135};
inline const sf::Color BevelShadow{15, 10, 22};

inline const sf::Color AccentColor{140, 80, 255};
inline const sf::Color AccentHoverColor{170, 115, 255};

inline const sf::Color TextPrimary{238, 232, 250};
inline const sf::Color TextSecondary{165, 150, 195};
inline const sf::Color TextMuted{105, 95, 130};
inline const sf::Color TextAccent{180, 120, 255};

inline constexpr float HeaderHeight = 28.0f;
inline constexpr float ToolbarHeight = 36.0f;
inline constexpr float StatusBarHeight = 24.0f;
inline constexpr float BorderThickness = 1.0f;
inline constexpr float CornerRadius = 0.0f;

}

}