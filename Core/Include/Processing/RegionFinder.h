#pragma once
#include <vector>

namespace StudioCore {

class SourceTexture;

// One separate thing left on a cleaned sheet: a sprite, a text label, a speck.
struct LooseRegion {
    int x{0}, y{0}, width{0}, height{0};
    std::vector<int> pixels; // indices into the texture (y * width + x)
};

class RegionFinder {
public:
    RegionFinder() = delete; // Static processing class

    // Splits the opaque pixels into separate objects. Pieces on the same row
    // that sit close together (the letters of a caption) count as one object;
    // nothing is joined vertically, so a caption under a sprite stays separate.
    // The largest maxRegions are returned, biggest first.
    static std::vector<LooseRegion> Find(const SourceTexture& source, int maxRegions = 120, int minArea = 12);
};

}
