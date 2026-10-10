#pragma once
#include <vector>
#include <cstdint>

namespace StudioCore {

class SourceTexture;

class BackgroundRemover {
public:
    BackgroundRemover() = delete; // Static processing class

    // Removes the background of a sprite sheet (solid colours, fake transparency
    // checkerboards, frame separator lines) and returns the cleaned RGBA pixels.
    // Returns the source pixels unchanged when no background can be identified,
    // so running it on an already clean sheet is a no-op.
    static std::vector<uint8_t> Clean(const SourceTexture& source);
};

}
