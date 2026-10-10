#include "Processing/RegionFinder.h"
#include "DataModels/SourceTexture.h"
#include <algorithm>
#include <numeric>

namespace StudioCore {

namespace {

int FindRoot(std::vector<int>& parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

void Unite(std::vector<int>& parent, int a, int b) {
    a = FindRoot(parent, a);
    b = FindRoot(parent, b);
    if (a != b) parent[std::max(a, b)] = std::min(a, b);
}

}

std::vector<LooseRegion> RegionFinder::Find(const SourceTexture& source, int maxRegions, int minArea) {
    std::vector<LooseRegion> regions;
    const int w = source.GetWidth();
    const int h = source.GetHeight();
    const auto& px = source.GetPixels();
    if (w <= 0 || h <= 0 || px.size() != static_cast<size_t>(w) * h * 4) return regions;

    const int count = w * h;
    auto opaque = [&](int i) { return px[static_cast<size_t>(i) * 4 + 3] > 0; };

    // Connected components (8-connected) with a two pass union-find.
    std::vector<int> label(count, -1);
    std::vector<int> parent;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            if (!opaque(i)) continue;

            int current = -1;
            auto join = [&](int n) {
                if (label[n] < 0) return;
                if (current < 0) current = label[n];
                else Unite(parent, current, label[n]);
            };
            if (x > 0) join(i - 1);
            if (y > 0) {
                join(i - w);
                if (x > 0) join(i - w - 1);
                if (x < w - 1) join(i - w + 1);
            }
            if (current < 0) {
                current = static_cast<int>(parent.size());
                parent.push_back(current);
            }
            label[i] = current;
        }
    }

    // Join pieces that are close together on the same row.
    const int gap = std::max(6, static_cast<int>(w * 0.005f));
    for (int y = 0; y < h; ++y) {
        int lastX = -1;
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            if (label[i] < 0) continue;
            if (lastX >= 0 && x - lastX > 1 && x - lastX <= gap) {
                Unite(parent, label[i], label[y * w + lastX]);
            }
            lastX = x;
        }
    }

    std::vector<int> regionOf(parent.size(), -1);
    for (int i = 0; i < count; ++i) {
        if (label[i] < 0) continue;
        const int root = FindRoot(parent, label[i]);
        if (regionOf[root] < 0) {
            regionOf[root] = static_cast<int>(regions.size());
            regions.emplace_back();
            regions.back().x = w;
            regions.back().y = h;
        }
        LooseRegion& region = regions[regionOf[root]];
        const int x = i % w;
        const int y = i / w;
        // width/height hold the far edge until the box is finished below
        region.x = std::min(region.x, x);
        region.y = std::min(region.y, y);
        region.width = std::max(region.width, x + 1);
        region.height = std::max(region.height, y + 1);
        region.pixels.push_back(i);
    }

    for (auto& region : regions) {
        region.width -= region.x;
        region.height -= region.y;
    }

    regions.erase(std::remove_if(regions.begin(), regions.end(),
        [minArea](const LooseRegion& r) { return static_cast<int>(r.pixels.size()) < minArea; }), regions.end());
    std::sort(regions.begin(), regions.end(),
        [](const LooseRegion& a, const LooseRegion& b) { return a.pixels.size() > b.pixels.size(); });
    if (maxRegions > 0 && static_cast<int>(regions.size()) > maxRegions) regions.resize(maxRegions);

    // Reading order is easier to follow on the numbered overview.
    std::sort(regions.begin(), regions.end(), [](const LooseRegion& a, const LooseRegion& b) {
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    return regions;
}

}
