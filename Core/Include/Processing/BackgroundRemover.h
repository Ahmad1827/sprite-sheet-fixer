#pragma once
#include <vector>
#include <cstdint>

namespace StudioCore {

class SourceTexture;

struct CleanOptions {
    bool softEdges{true};         // partial transparency on silhouettes; off keeps hard pixel edges
    bool removeFrameLines{true};  // delete separator lines drawn between frames
    int maxSpeckArea{0};          // also delete isolated leftovers up to this many pixels (0 = off)
};

class BackgroundRemover {
public:
    BackgroundRemover() = delete; // Static processing class

    // Removes the background of a sprite sheet (solid colours, fake transparency
    // checkerboards, frame separator lines) and returns the cleaned RGBA pixels.
    // Returns the source pixels unchanged when no background can be identified,
    // so running it on an already clean sheet is a no-op.
    static std::vector<uint8_t> Clean(const SourceTexture& source, const CleanOptions& options = {});

    // Deletes isolated groups of pixels no larger than maxArea. Works on any
    // sheet, including one that has already been cleaned.
    static void RemoveSpecks(std::vector<uint8_t>& pixels, int width, int height, int maxArea);
};

}
