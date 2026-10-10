#pragma once
#include <SFML/Graphics/Color.hpp>

namespace StudioUI {

namespace Theme {

inline const sf::Color MainBackground{20, 22, 26};
inline const sf::Color PanelBackground{30, 33, 39};
inline const sf::Color InspectorBackground{25, 27, 32};
inline const sf::Color ViewportBackground{15, 16, 19};
inline const sf::Color BorderColor{54, 58, 68};
inline const sf::Color HoverColor{46, 50, 60};
inline const sf::Color ActiveColor{66, 153, 255};

inline const sf::Color BevelLight{70, 76, 88};
inline const sf::Color BevelShadow{12, 13, 16};

inline const sf::Color AccentColor{66, 153, 255};
inline const sf::Color AccentHoverColor{120, 184, 255};
inline const sf::Color DangerColor{235, 87, 87};
inline const sf::Color SuccessColor{72, 199, 142};

inline const sf::Color TextPrimary{234, 237, 242};
inline const sf::Color TextSecondary{164, 171, 184};
inline const sf::Color TextMuted{112, 119, 133};
inline const sf::Color TextAccent{120, 184, 255};

inline constexpr float HeaderHeight = 28.0f;
inline constexpr float ToolbarHeight = 60.0f;
inline constexpr float StatusBarHeight = 30.0f;
inline constexpr float BorderThickness = 1.0f;
inline constexpr float CornerRadius = 6.0f;

// Space the animation list/preview and the timeline take when they are shown.
inline constexpr float SidePanelWidth = 300.0f;
inline constexpr float TimelineHeight = 160.0f;

}

}
