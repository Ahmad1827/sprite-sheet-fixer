#include "Processing/BackgroundRemover.h"
#include "DataModels/SourceTexture.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace StudioCore {

namespace {

struct Rgb {
    float r{0.0f}, g{0.0f}, b{0.0f};
};

struct Cluster {
    Rgb leader;
    Rgb mean;
    double sumR{0.0}, sumG{0.0}, sumB{0.0};
    long count{0};
    float tolerance{0.0f};
};

// Distances to the background model are stored per pixel as a byte where
// kAtTolerance means "exactly at the edge of the cluster tolerance".
constexpr int kAtTolerance = 32;
constexpr int kDriftLimit = 80;
constexpr size_t kMaxClusters = 6;

// Pixel removal reasons.
constexpr uint8_t kKept = 0;
constexpr uint8_t kBackground = 1;
constexpr uint8_t kSeparator = 2;

inline float DistSq(const Rgb& a, const Rgb& b) {
    float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

inline Rgb ColorAt(const std::vector<uint8_t>& px, int i) {
    size_t idx = static_cast<size_t>(i) * 4;
    return { static_cast<float>(px[idx]), static_cast<float>(px[idx + 1]), static_cast<float>(px[idx + 2]) };
}

inline uint8_t ToByte(float v) {
    return static_cast<uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f));
}

// Builds the background palette from the colours that dominate the image border.
// Colours that only touch a small part of the border (a sprite cropped by the
// edge, a frame line) are not considered background.
std::vector<Cluster> BuildBorderPalette(const std::vector<uint8_t>& px, int w, int h, bool pixelArt) {
    struct Bin {
        long count{0};
        double r{0.0}, g{0.0}, b{0.0};
    };
    std::vector<Bin> bins(32 * 32 * 32);
    std::vector<int> ring;
    ring.reserve(static_cast<size_t>(w + h) * 2);
    for (int x = 0; x < w; ++x) {
        ring.push_back(x);
        if (h > 1) ring.push_back((h - 1) * w + x);
    }
    for (int y = 1; y < h - 1; ++y) {
        ring.push_back(y * w);
        if (w > 1) ring.push_back(y * w + (w - 1));
    }

    size_t transparentSamples = 0;
    for (int i : ring) {
        size_t idx = static_cast<size_t>(i) * 4;
        if (px[idx + 3] == 0) {
            ++transparentSamples;
            continue;
        }
        Bin& bin = bins[(px[idx] >> 3) * 1024 + (px[idx + 1] >> 3) * 32 + (px[idx + 2] >> 3)];
        bin.count++;
        bin.r += px[idx];
        bin.g += px[idx + 1];
        bin.b += px[idx + 2];
    }

    // A border that is already mostly transparent means the sheet has been
    // cleaned before; whatever still touches the border is artwork.
    if (transparentSamples * 2 >= ring.size()) return {};

    std::vector<const Bin*> used;
    for (const auto& bin : bins) {
        if (bin.count > 0) used.push_back(&bin);
    }
    std::sort(used.begin(), used.end(), [](const Bin* a, const Bin* b) { return a->count > b->count; });

    const float mergeRadiusSq = 30.0f * 30.0f;
    std::vector<Cluster> clusters;
    for (const Bin* bin : used) {
        Rgb mean{ static_cast<float>(bin->r / bin->count), static_cast<float>(bin->g / bin->count), static_cast<float>(bin->b / bin->count) };
        Cluster* target = nullptr;
        for (auto& c : clusters) {
            if (DistSq(c.leader, mean) <= mergeRadiusSq) {
                target = &c;
                break;
            }
        }
        if (!target) {
            if (clusters.size() >= 16) continue;
            clusters.emplace_back();
            target = &clusters.back();
            target->leader = mean;
        }
        target->count += bin->count;
        target->sumR += bin->r;
        target->sumG += bin->g;
        target->sumB += bin->b;
    }

    const long minCount = std::max<long>(8, static_cast<long>(ring.size() * 0.08));
    clusters.erase(std::remove_if(clusters.begin(), clusters.end(),
        [minCount](const Cluster& c) { return c.count < minCount; }), clusters.end());
    std::sort(clusters.begin(), clusters.end(), [](const Cluster& a, const Cluster& b) { return a.count > b.count; });
    if (clusters.size() > kMaxClusters) clusters.resize(kMaxClusters);

    for (auto& c : clusters) {
        c.mean = { static_cast<float>(c.sumR / c.count), static_cast<float>(c.sumG / c.count), static_cast<float>(c.sumB / c.count) };
    }

    // Measure how noisy each background colour is so the tolerance fits the image:
    // tight for flat colours, wider for compressed or AI generated backgrounds.
    // The median is used so blurred tile edges on the border do not inflate it.
    std::vector<std::vector<float>> spread(clusters.size());
    for (int i : ring) {
        size_t idx = static_cast<size_t>(i) * 4;
        if (px[idx + 3] == 0) continue;
        Rgb c = ColorAt(px, i);
        int nearest = -1;
        float best = 45.0f * 45.0f;
        for (size_t k = 0; k < clusters.size(); ++k) {
            float d = DistSq(clusters[k].mean, c);
            if (d < best) {
                best = d;
                nearest = static_cast<int>(k);
            }
        }
        if (nearest >= 0) spread[nearest].push_back(std::sqrt(best));
    }
    for (size_t k = 0; k < clusters.size(); ++k) {
        float median = 0.0f;
        if (!spread[k].empty()) {
            auto mid = spread[k].begin() + spread[k].size() / 2;
            std::nth_element(spread[k].begin(), mid, spread[k].end());
            median = *mid;
        }
        clusters[k].tolerance = pixelArt ? std::clamp(3.0f * median + 3.0f, 3.0f, 36.0f)
                                         : std::clamp(3.0f * median + 6.0f, 10.0f, 36.0f);
    }
    return clusters;
}

bool IsPixelArt(const std::vector<uint8_t>& px, int count) {
    std::unordered_set<uint32_t> colors;
    for (int i = 0; i < count; ++i) {
        size_t idx = static_cast<size_t>(i) * 4;
        if (px[idx + 3] == 0) continue;
        colors.insert((static_cast<uint32_t>(px[idx]) << 16) | (static_cast<uint32_t>(px[idx + 1]) << 8) | px[idx + 2]);
        if (colors.size() > 1024) return false;
    }
    return true;
}

// Removes long thin uniform lines (frame separators drawn between sprites).
// vertical == true scans for vertical lines, otherwise horizontal ones.
void RemoveSeparatorLines(const std::vector<uint8_t>& px, int w, int h, bool vertical, std::vector<uint8_t>& gone) {
    const int lines = vertical ? w : h;
    const int length = vertical ? h : w;
    auto at = [&](int line, int pos) { return vertical ? pos * w + line : line * w + pos; };
    auto remaining = [&](int i) { return px[static_cast<size_t>(i) * 4 + 3] > 0 && gone[i] == kKept; };

    std::vector<int> coverage(lines, 0);
    for (int line = 0; line < lines; ++line) {
        int c = 0;
        for (int pos = 0; pos < length; ++pos) {
            if (remaining(at(line, pos))) ++c;
        }
        coverage[line] = c;
    }

    const int minCoverage = static_cast<int>(length * 0.7f);
    const int maxBand = std::max(6, static_cast<int>(std::min(w, h) * 0.014f + 0.5f));
    const int margin = 3;

    struct Band {
        int first, last;
        Rgb color;
    };
    std::vector<Band> bands;

    for (int line = 0; line < lines;) {
        if (coverage[line] < minCoverage) {
            ++line;
            continue;
        }
        int first = line;
        while (line < lines && coverage[line] >= minCoverage) ++line;
        int last = line - 1;
        if (last - first + 1 > maxBand) continue;

        int core = first;
        for (int l = first; l <= last; ++l) {
            if (coverage[l] > coverage[core]) core = l;
        }

        double sr = 0.0, sg = 0.0, sb = 0.0;
        for (int pos = 0; pos < length; ++pos) {
            int i = at(core, pos);
            if (!remaining(i)) continue;
            Rgb c = ColorAt(px, i);
            sr += c.r;
            sg += c.g;
            sb += c.b;
        }
        Rgb mean{ static_cast<float>(sr / coverage[core]), static_cast<float>(sg / coverage[core]), static_cast<float>(sb / coverage[core]) };

        int uniform = 0;
        for (int pos = 0; pos < length; ++pos) {
            int i = at(core, pos);
            if (remaining(i) && DistSq(ColorAt(px, i), mean) <= 48.0f * 48.0f) ++uniform;
        }
        if (uniform < coverage[core] * 0.85f) continue;

        // A separator stands alone: just beside it the sheet is mostly empty.
        // A tall sprite also fills its columns, but so do the columns next to it.
        const int before = first - margin - 1;
        const int after = last + margin + 1;
        const int flank = std::max(before >= 0 ? coverage[before] : 0, after < lines ? coverage[after] : 0);
        if (flank > coverage[core] * 0.3f) continue;

        bands.push_back({ first, last, mean });
    }

    for (const auto& band : bands) {
        const int lo = std::max(0, band.first - margin);
        const int hi = std::min(lines - 1, band.last + margin);

        for (int pos = 0; pos < length; ++pos) {
            for (int line = lo; line <= hi;) {
                if (!remaining(at(line, pos))) {
                    ++line;
                    continue;
                }
                int runFirst = line;
                while (line <= hi && remaining(at(line, pos))) ++line;
                int runLast = line - 1;

                // A run that continues past the band belongs to a sprite crossing
                // the line: only the line coloured pixels inside the band go.
                bool attached = (runFirst == lo && lo > 0 && remaining(at(lo - 1, pos))) ||
                                (runLast == hi && hi < lines - 1 && remaining(at(hi + 1, pos)));

                for (int l = runFirst; l <= runLast; ++l) {
                    int i = at(l, pos);
                    if (attached) {
                        if (l < band.first || l > band.last) continue;
                        if (DistSq(ColorAt(px, i), band.color) > 60.0f * 60.0f) continue;
                    }
                    gone[i] = kSeparator;
                }
            }
        }
    }
}

}

void BackgroundRemover::RemoveSpecks(std::vector<uint8_t>& pixels, int width, int height, int maxArea) {
    if (maxArea <= 0 || width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * height * 4) return;

    const int count = width * height;
    std::vector<uint8_t> visited(count, 0);
    std::vector<int> component;
    const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    const int dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

    for (int start = 0; start < count; ++start) {
        if (visited[start] || pixels[static_cast<size_t>(start) * 4 + 3] == 0) continue;

        component.clear();
        component.push_back(start);
        visited[start] = 1;
        for (size_t head = 0; head < component.size(); ++head) {
            const int cur = component[head];
            const int cx = cur % width;
            const int cy = cur / width;
            for (int d = 0; d < 8; ++d) {
                const int nx = cx + dx[d];
                const int ny = cy + dy[d];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                const int n = ny * width + nx;
                if (visited[n] || pixels[static_cast<size_t>(n) * 4 + 3] == 0) continue;
                visited[n] = 1;
                component.push_back(n);
            }
        }

        if (static_cast<int>(component.size()) > maxArea) continue;
        for (int i : component) {
            const size_t idx = static_cast<size_t>(i) * 4;
            pixels[idx] = 0;
            pixels[idx + 1] = 0;
            pixels[idx + 2] = 0;
            pixels[idx + 3] = 0;
        }
    }
}

std::vector<uint8_t> BackgroundRemover::Clean(const SourceTexture& source, const CleanOptions& options) {
    const int w = source.GetWidth();
    const int h = source.GetHeight();
    const std::vector<uint8_t>& px = source.GetPixels();
    if (w < 2 || h < 2 || px.size() != static_cast<size_t>(w) * h * 4) return px;

    const int count = w * h;
    auto opaque = [&](int i) { return px[static_cast<size_t>(i) * 4 + 3] > 0; };

    const bool pixelArt = IsPixelArt(px, count);
    const std::vector<Cluster> palette = BuildBorderPalette(px, w, h, pixelArt);
    if (palette.empty()) return px;

    const int clusterCount = static_cast<int>(palette.size());

    // --- 1. Distance of every pixel to the background model ---------------------
    // The model is the palette colours plus the straight blends between them, which
    // covers the blurred tile edges of a checkerboard and soft gradients.
    struct Segment {
        Rgb a, d;
        float lenSq, tolA, tolB;
        float extend; // how far past either end the tone line is followed, in t units
    };
    std::vector<Segment> segments;
    for (int i = 0; i < clusterCount; ++i) {
        for (int j = i + 1; j < clusterCount; ++j) {
            Rgb d{ palette[j].mean.r - palette[i].mean.r, palette[j].mean.g - palette[i].mean.g, palette[j].mean.b - palette[i].mean.b };
            float lenSq = d.r * d.r + d.g * d.g + d.b * d.b;
            if (lenSq < 1.0f) continue;
            float extend = std::min(0.5f, 1.5f * (palette[i].tolerance + palette[j].tolerance) / std::sqrt(lenSq));
            segments.push_back({ palette[i].mean, d, lenSq, palette[i].tolerance, palette[j].tolerance, extend });
        }
    }

    // Offset of a colour from the tone line (the palette colours, the blends
    // between them and a little beyond either end). Background that an AI
    // generator smudged lighter or darker still sits on this line; sprite colours
    // that merely look similar are tinted and sit off it.
    auto lineOffset = [&](const Rgb& c, Rgb& offset) -> bool {
        float best = 1e9f;
        bool within = false;
        for (const auto& s : segments) {
            float t = ((c.r - s.a.r) * s.d.r + (c.g - s.a.g) * s.d.g + (c.b - s.a.b) * s.d.b) / s.lenSq;
            t = std::clamp(t, -s.extend, 1.0f + s.extend);
            Rgb o{ c.r - (s.a.r + s.d.r * t), c.g - (s.a.g + s.d.g * t), c.b - (s.a.b + s.d.b * t) };
            float d = o.r * o.r + o.g * o.g + o.b * o.b;
            if (d < best) {
                best = d;
                offset = o;
                float tol = s.tolA + (s.tolB - s.tolA) * std::clamp(t, 0.0f, 1.0f);
                within = d <= tol * tol;
            }
        }
        return within;
    };

    // modelDist is the distance to the nearest palette colour. A blend of two
    // palette colours is only promoted to background when it really sits on a
    // boundary between both tones; otherwise a grey rock on a grey checkerboard
    // would count as background just because its colour lies between them.
    std::vector<uint8_t> modelDist(count, 255);
    std::vector<uint8_t> onToneLine(segments.empty() ? 0 : count, 0);
    std::vector<uint8_t> nearestCluster(count, 0);
    std::vector<int> blendCandidates;
    for (int i = 0; i < count; ++i) {
        if (!opaque(i)) continue;
        Rgb c = ColorAt(px, i);

        float best = 1e9f;
        float bestRaw = 1e9f;
        for (int k = 0; k < clusterCount; ++k) {
            float raw = DistSq(c, palette[k].mean);
            if (raw < bestRaw) {
                bestRaw = raw;
                nearestCluster[i] = static_cast<uint8_t>(k);
            }
            float n = std::sqrt(raw) / palette[k].tolerance;
            if (n < best) best = n;
        }
        modelDist[i] = static_cast<uint8_t>(std::min(255.0f, best * kAtTolerance));

        if (!segments.empty()) {
            Rgb offset;
            onToneLine[i] = (best <= 1.0f || lineOffset(c, offset)) ? 1 : 0;
        }

        if (best > 1.0f) {
            for (const auto& s : segments) {
                float t = ((c.r - s.a.r) * s.d.r + (c.g - s.a.g) * s.d.g + (c.b - s.a.b) * s.d.b) / s.lenSq;
                t = std::clamp(t, 0.0f, 1.0f);
                Rgb p{ s.a.r + s.d.r * t, s.a.g + s.d.g * t, s.a.b + s.d.b * t };
                if (std::sqrt(DistSq(c, p)) <= s.tolA + (s.tolB - s.tolA) * t) {
                    blendCandidates.push_back(i);
                    break;
                }
            }
        }
    }

    // A blend only counts as background when both tones are right next to it.
    {
        const int reach = 3;
        std::vector<int> confirmedBlends;
        for (int i : blendCandidates) {
            const int cx = i % w;
            const int cy = i / w;
            int firstTone = -1;
            bool boundary = false;
            for (int oy = -reach; oy <= reach && !boundary; ++oy) {
                for (int ox = -reach; ox <= reach; ++ox) {
                    int nx = cx + ox;
                    int ny = cy + oy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (!opaque(n) || modelDist[n] > kAtTolerance) continue;
                    if (firstTone < 0) {
                        firstTone = nearestCluster[n];
                    } else if (nearestCluster[n] != firstTone) {
                        boundary = true;
                        break;
                    }
                }
            }
            if (boundary) confirmedBlends.push_back(i);
        }
        for (int i : confirmedBlends) modelDist[i] = kAtTolerance;
    }

    // --- 2. Flood the exterior from the image border ----------------------------
    float averageTolerance = 0.0f;
    for (const auto& c : palette) averageTolerance += c.tolerance;
    averageTolerance /= clusterCount;
    const float stepTol = std::clamp(averageTolerance * 0.3f, 4.0f, 9.0f);
    const float stepTolSq = stepTol * stepTol;

    std::vector<uint8_t> exterior(count, 0);
    std::vector<int> queue;
    queue.reserve(count / 4);

    auto seed = [&](int i) {
        if (exterior[i]) return;
        if (!opaque(i) || modelDist[i] <= kAtTolerance) {
            exterior[i] = 1;
            queue.push_back(i);
        }
    };
    for (int x = 0; x < w; ++x) {
        seed(x);
        seed((h - 1) * w + x);
    }
    for (int y = 0; y < h; ++y) {
        seed(y * w);
        seed(y * w + (w - 1));
    }

    const int dx4[4] = { 1, -1, 0, 0 };
    const int dy4[4] = { 0, 0, 1, -1 };
    const int dx8[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    const int dy8[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

    for (size_t head = 0; head < queue.size(); ++head) {
        const int cur = queue[head];
        const int cx = cur % w;
        const int cy = cur / w;
        const bool curOpaque = opaque(cur);

        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx4[d];
            int ny = cy + dy4[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int n = ny * w + nx;
            if (exterior[n]) continue;

            bool accept = !opaque(n) || modelDist[n] <= kAtTolerance;
            // Follow slow drifts of the background (vignettes, soft smudges) that
            // stay close to the palette, without jumping over a real edge.
            if (!accept && !pixelArt && curOpaque) {
                const bool nearModel = segments.empty() ? modelDist[n] <= kDriftLimit : onToneLine[n] != 0;
                accept = nearModel && DistSq(ColorAt(px, n), ColorAt(px, cur)) <= stepTolSq;
            }
            if (accept) {
                exterior[n] = 1;
                queue.push_back(n);
            }
        }
    }

    std::vector<uint8_t> gone(count, kKept);
    for (int i = 0; i < count; ++i) {
        if (exterior[i] && opaque(i)) gone[i] = kBackground;
    }

    // --- 3. Background trapped inside sprites (between legs, under arms) --------
    // Regions are grouped diagonally so checker tiles that only touch at a corner
    // still count as one. How readily a trapped region is removed depends on how
    // likely a sprite is to contain the background colour itself:
    //  - checkerboard: removed when it shows both tones or is large;
    //  - solid saturated colour (magenta, green...): always removed;
    //  - solid white/grey/black: removed only when it is a noticeable share of the
    //    sprite around it, because tiny ones are usually eyes, teeth or highlights.
    const int minHoleArea = std::clamp(static_cast<int>(count * 0.00004f), 16, 400);

    // Checkerboard tile size, measured as the typical run of one tone along the
    // border. Used to judge pockets of background that an outline has cut off.
    int tileSize = 0;
    if (clusterCount >= 2) {
        std::vector<int> runs;
        auto scan = [&](int start, int stride, int length) {
            int run = 0;
            int tone = -1;
            for (int k = 0; k < length; ++k) {
                int i = start + k * stride;
                int t = (opaque(i) && modelDist[i] <= kAtTolerance) ? nearestCluster[i] : -1;
                if (t == tone) {
                    ++run;
                    continue;
                }
                if (tone >= 0 && run >= 2) runs.push_back(run);
                tone = t;
                run = 1;
            }
        };
        scan(0, 1, w);
        scan((h - 1) * w, 1, w);
        scan(0, w, h);
        scan(w - 1, w, h);
        if (runs.size() >= 8) {
            std::nth_element(runs.begin(), runs.begin() + runs.size() / 2, runs.end());
            tileSize = runs[runs.size() / 2];
        }
    }
    const int minPocketArea = std::max(12, tileSize * tileSize / 8);
    std::vector<uint8_t> visited(count, 0);
    std::vector<uint8_t> inComponent(count, 0);
    std::vector<int> component;
    std::vector<std::vector<int>> smallPockets;
    std::vector<std::vector<int>> undecided;

    const bool solidBackground = clusterCount == 1;
    bool chromaticBackground = false;
    if (solidBackground) {
        const Rgb& c = palette[0].mean;
        chromaticBackground = std::max({ c.r, c.g, c.b }) - std::min({ c.r, c.g, c.b }) >= 50.0f;
    }
    for (int start = 0; start < count; ++start) {
        if (visited[start] || exterior[start] || !opaque(start) || modelDist[start] > kAtTolerance) continue;

        component.clear();
        component.push_back(start);
        visited[start] = 1;
        long perCluster[kMaxClusters] = { 0 };
        double toneSum[kMaxClusters][3] = { { 0.0 } };

        for (size_t head = 0; head < component.size(); ++head) {
            int cur = component[head];
            const int tone = nearestCluster[cur];
            const size_t curIdx = static_cast<size_t>(cur) * 4;
            perCluster[tone]++;
            toneSum[tone][0] += px[curIdx];
            toneSum[tone][1] += px[curIdx + 1];
            toneSum[tone][2] += px[curIdx + 2];
            int cx = cur % w;
            int cy = cur / w;
            for (int d = 0; d < 8; ++d) {
                int nx = cx + dx8[d];
                int ny = cy + dy8[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int n = ny * w + nx;
                if (visited[n] || exterior[n] || !opaque(n) || modelDist[n] > kAtTolerance) continue;
                visited[n] = 1;
                component.push_back(n);
            }
        }

        // The rim of a trapped region is where the background blends into the
        // sprite, so its colours are off. Measure the region by its core (pixels
        // with no sprite beside them) whenever it has one.
        long corePer[kMaxClusters] = { 0 };
        double coreSum[kMaxClusters][3] = { { 0.0 } };
        bool hasCore = false;
        for (int i : component) inComponent[i] = 1;
        for (int i : component) {
            const int cx = i % w;
            const int cy = i / w;
            bool core = true;
            for (int d = 0; d < 4 && core; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                core = inComponent[ny * w + nx] != 0;
            }
            if (!core) continue;
            const int tone = nearestCluster[i];
            const size_t idx = static_cast<size_t>(i) * 4;
            corePer[tone]++;
            coreSum[tone][0] += px[idx];
            coreSum[tone][1] += px[idx + 1];
            coreSum[tone][2] += px[idx + 2];
        }
        for (int i : component) inComponent[i] = 0;
        for (int k = 0; k < clusterCount; ++k) {
            if (corePer[k] >= 4) hasCore = true;
        }

        const long area = static_cast<long>(component.size());
        int tones = 0;
        bool averagesMatch = true;
        for (int k = 0; k < clusterCount; ++k) {
            if (perCluster[k] < area * 0.15f) continue;
            ++tones;
            // Individual pixels may sit anywhere inside the tolerance, but the
            // average of real background lands on the background colour. A sprite
            // area that is merely similar (a grey rock shadow, a cream eye) is
            // tinted, so its average is off and it is left alone.
            const bool useCore = corePer[k] >= 4;
            if (hasCore && !useCore) continue;   // this tone is rim only; the core tones decide
            const double* sum = useCore ? coreSum[k] : toneSum[k];
            const long n = useCore ? corePer[k] : perCluster[k];
            Rgb average{ static_cast<float>(sum[0] / n), static_cast<float>(sum[1] / n), static_cast<float>(sum[2] / n) };
            const float limit = std::max(5.0f, palette[k].tolerance * 0.4f);
            if (DistSq(average, palette[k].mean) > limit * limit) averagesMatch = false;
        }
        if (!averagesMatch) continue;

        if (solidBackground) {
            if (chromaticBackground) {
                if (area >= 4) {
                    for (int i : component) gone[i] = kBackground;
                }
            } else if (area >= 16) {
                undecided.push_back(component);
            }
        } else if (area >= minHoleArea || (tones >= 2 && area >= 24)) {
            for (int i : component) gone[i] = kBackground;
        } else if (!pixelArt && area >= minPocketArea) {
            long distSum = 0;
            for (int i : component) distSum += modelDist[i];
            if (distSum <= area * (kAtTolerance * 3 / 4)) smallPockets.push_back(component);
        }
    }

    // Solid white/grey/black background: compare each trapped region with the
    // size of the sprite that encloses it.
    if (!undecided.empty()) {
        std::vector<int> label(count, -1);
        std::vector<long> spriteArea;
        std::vector<int> stack;
        for (int start = 0; start < count; ++start) {
            if (label[start] >= 0 || exterior[start] || !opaque(start)) continue;
            const int id = static_cast<int>(spriteArea.size());
            long area = 0;
            stack.clear();
            stack.push_back(start);
            label[start] = id;
            while (!stack.empty()) {
                int cur = stack.back();
                stack.pop_back();
                ++area;
                int cx = cur % w;
                int cy = cur / w;
                for (int d = 0; d < 8; ++d) {
                    int nx = cx + dx8[d];
                    int ny = cy + dy8[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (label[n] >= 0 || exterior[n] || !opaque(n)) continue;
                    label[n] = id;
                    stack.push_back(n);
                }
            }
            spriteArea.push_back(area);
        }
        for (const auto& region : undecided) {
            const long enclosing = spriteArea[label[region.front()]];
            if (static_cast<long>(region.size()) * 400 >= enclosing) {
                for (int i : region) gone[i] = kBackground;
            }
        }
    }

    // On a checkerboard a small pocket is still background when the other
    // checker tone sits right beside it, either already removed or as another
    // pocket (tiles cut off by an outline thinner than a tile). Never applied to
    // solid backgrounds, where a pocket this small is usually a real detail that
    // happens to share the background colour.
    if (!smallPockets.empty()) {
        const int reach = std::clamp(tileSize * 3 / 4, 3, 24);
        std::vector<uint8_t> inPocket(count, 0);
        for (const auto& pocket : smallPockets) {
            for (int i : pocket) inPocket[i] = 1;
        }
        std::vector<uint8_t> confirmed(smallPockets.size(), 0);
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t p = 0; p < smallPockets.size(); ++p) {
                if (confirmed[p]) continue;
                const uint8_t tone = nearestCluster[smallPockets[p].front()];
                bool found = false;
                for (int i : smallPockets[p]) {
                    int cx = i % w;
                    int cy = i / w;
                    for (int oy = -reach; oy <= reach && !found; ++oy) {
                        for (int ox = -reach; ox <= reach; ++ox) {
                            int nx = cx + ox;
                            int ny = cy + oy;
                            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                            int n = ny * w + nx;
                            if ((gone[n] == kBackground || inPocket[n]) && nearestCluster[n] != tone) {
                                found = true;
                                break;
                            }
                        }
                    }
                    if (found) break;
                }
                if (found) {
                    confirmed[p] = 1;
                    changed = true;
                    for (int i : smallPockets[p]) gone[i] = kBackground;
                }
            }
        }
    }

    // --- 3b. Off-tone background -----------------------------------------------------
    // Generated checkerboards contain tiles and smudges that are lighter or darker
    // than the two real tones. They are still neutral, so they sit on the tone
    // line. A thick region of such pixels that touches removed background and
    // whose average is not tinted is background too. Thin rings are left for the
    // soft edge pass, tinted regions (rock, stone, fur) are sprite.
    if (!segments.empty() && !pixelArt) {
        auto candidate = [&](int i) { return opaque(i) && gone[i] == kKept && onToneLine[i]; };

        std::vector<std::vector<int>> regions;
        std::fill(visited.begin(), visited.end(), 0);
        for (int start = 0; start < count; ++start) {
            if (visited[start] || !candidate(start)) continue;

            component.clear();
            component.push_back(start);
            visited[start] = 1;
            double offR = 0.0, offG = 0.0, offB = 0.0;
            bool thick = false;

            for (size_t head = 0; head < component.size(); ++head) {
                int cur = component[head];
                int cx = cur % w;
                int cy = cur / w;

                Rgb offset{};
                lineOffset(ColorAt(px, cur), offset);
                offR += offset.r;
                offG += offset.g;
                offB += offset.b;

                if (!thick && cx >= 2 && cy >= 2 && cx < w - 2 && cy < h - 2) {
                    bool full = true;
                    for (int oy = -2; oy <= 2 && full; ++oy) {
                        for (int ox = -2; ox <= 2; ++ox) {
                            if (!candidate((cy + oy) * w + cx + ox)) {
                                full = false;
                                break;
                            }
                        }
                    }
                    thick = full;
                }

                for (int d = 0; d < 8; ++d) {
                    int nx = cx + dx8[d];
                    int ny = cy + dy8[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (visited[n] || !candidate(n)) continue;
                    visited[n] = 1;
                    component.push_back(n);
                }
            }

            if (!thick) continue;
            const double area = static_cast<double>(component.size());
            const double mr = offR / area, mg = offG / area, mb = offB / area;
            const float limit = std::max(6.0f, averageTolerance * 0.55f);
            if (mr * mr + mg * mg + mb * mb > limit * limit) continue;
            regions.push_back(component);
        }

        std::vector<uint8_t> accepted(regions.size(), 0);
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t r = 0; r < regions.size(); ++r) {
                if (accepted[r]) continue;
                bool touches = false;
                for (int i : regions[r]) {
                    int cx = i % w;
                    int cy = i / w;
                    for (int d = 0; d < 8; ++d) {
                        int nx = cx + dx8[d];
                        int ny = cy + dy8[d];
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        int n = ny * w + nx;
                        if (!opaque(n) || gone[n] == kBackground) {
                            touches = true;
                            break;
                        }
                    }
                    if (touches) break;
                }
                if (touches) {
                    accepted[r] = 1;
                    changed = true;
                    for (int i : regions[r]) gone[i] = kBackground;
                }
            }
        }
    }

    // --- 4. Frame separator lines ------------------------------------------------
    if (options.removeFrameLines) {
        RemoveSeparatorLines(px, w, h, true, gone);
        RemoveSeparatorLines(px, w, h, false, gone);
    }

    auto remaining = [&](int i) { return opaque(i) && gone[i] == kKept; };

    // --- 5. Leftover islands --------------------------------------------------------
    // Islands made only of background-like colours, and specks left behind where a
    // separator line crossed a sprite.
    std::fill(visited.begin(), visited.end(), 0);
    for (int start = 0; start < count; ++start) {
        if (visited[start] || !remaining(start)) continue;

        component.clear();
        component.push_back(start);
        visited[start] = 1;
        long backgroundLike = 0;
        bool touchesSeparator = false;

        for (size_t head = 0; head < component.size(); ++head) {
            int cur = component[head];
            if (modelDist[cur] <= kDriftLimit) ++backgroundLike;
            int cx = cur % w;
            int cy = cur / w;
            for (int d = 0; d < 8; ++d) {
                int nx = cx + dx8[d];
                int ny = cy + dy8[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int n = ny * w + nx;
                if (gone[n] == kSeparator) touchesSeparator = true;
                if (visited[n] || !remaining(n)) continue;
                visited[n] = 1;
                component.push_back(n);
            }
        }

        const long area = static_cast<long>(component.size());
        bool remove = touchesSeparator && area <= 24;
        if (!pixelArt && backgroundLike >= static_cast<long>(area * 0.97f)) remove = true;
        if (remove) {
            for (int i : component) gone[i] = kBackground;
        }
    }

    std::vector<uint8_t> result = px;
    for (int i = 0; i < count; ++i) {
        if (gone[i] == kKept) continue;
        size_t idx = static_cast<size_t>(i) * 4;
        result[idx] = 0;
        result[idx + 1] = 0;
        result[idx + 2] = 0;
        result[idx + 3] = 0;
    }

    if (pixelArt || !options.softEdges) return result;

    // --- 6. Soft edges ------------------------------------------------------------
    // Pixels on the silhouette are a blend of sprite and background. Work out how
    // much background is in each one, turn that into alpha and take the background
    // colour back out, so no halo is left on any backdrop.
    const uint8_t kInterior = 255;
    const int maxLayer = 3;
    std::vector<uint8_t> layer(count, kInterior);
    std::vector<int> frontier;
    for (int i = 0; i < count; ++i) {
        if (!remaining(i)) continue;
        int cx = i % w;
        int cy = i / w;
        for (int d = 0; d < 8; ++d) {
            int nx = cx + dx8[d];
            int ny = cy + dy8[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (gone[ny * w + nx] != kKept) {
                layer[i] = 1;
                frontier.push_back(i);
                break;
            }
        }
    }
    std::vector<std::vector<int>> layers(maxLayer + 1);
    layers[1] = frontier;
    for (int l = 2; l <= maxLayer; ++l) {
        for (int i : layers[l - 1]) {
            int cx = i % w;
            int cy = i / w;
            for (int d = 0; d < 8; ++d) {
                int nx = cx + dx8[d];
                int ny = cy + dy8[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int n = ny * w + nx;
                if (remaining(n) && layer[n] == kInterior) {
                    layer[n] = static_cast<uint8_t>(l);
                    layers[l].push_back(n);
                }
            }
        }
    }

    // Coverage (0..255) decided for each processed edge pixel; 255 = untouched.
    std::vector<uint8_t> coverage(count, 255);

    for (int l = 1; l <= maxLayer; ++l) {
        for (int i : layers[l]) {
            const int cx = i % w;
            const int cy = i / w;

            // Deeper layers only soften when they continue a ramp that started at
            // the silhouette, so crisp art keeps its interior untouched.
            float outerCoverage = 0.0f;
            if (l > 1) {
                int lowest = 255;
                for (int d = 0; d < 8; ++d) {
                    int nx = cx + dx8[d];
                    int ny = cy + dy8[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (layer[n] == l - 1) lowest = std::min<int>(lowest, coverage[n]);
                }
                if (lowest > 230) continue;
                outerCoverage = lowest / 255.0f;
            }

            // A colour that carries on into the sprite behind this pixel is the
            // sprite's own colour, not a blend with the background: leave it solid.
            {
                const Rgb own = ColorAt(px, i);
                bool continues = false;
                for (int d = 0; d < 8 && !continues; ++d) {
                    int nx = cx + dx8[d];
                    int ny = cy + dy8[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (!remaining(n) || layer[n] <= l) continue;
                    continues = DistSq(own, ColorAt(px, n)) <= 18.0f * 18.0f;
                }
                if (continues) continue;
            }

            // Local background colour: what was removed right next to this pixel.
            const int bgRadius = l + 1;
            double br = 0.0, bg = 0.0, bb = 0.0;
            int bgSamples = 0;
            for (int oy = -bgRadius; oy <= bgRadius; ++oy) {
                for (int ox = -bgRadius; ox <= bgRadius; ++ox) {
                    int nx = cx + ox;
                    int ny = cy + oy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int n = ny * w + nx;
                    if (gone[n] != kBackground) continue;
                    size_t nIdx = static_cast<size_t>(n) * 4;
                    br += px[nIdx];
                    bg += px[nIdx + 1];
                    bb += px[nIdx + 2];
                    ++bgSamples;
                }
            }
            if (bgSamples == 0) continue;
            const Rgb B{ static_cast<float>(br / bgSamples), static_cast<float>(bg / bgSamples), static_cast<float>(bb / bgSamples) };

            const Rgb C = ColorAt(px, i);
            const Rgb cb{ C.r - B.r, C.g - B.g, C.b - B.b };
            const float cbLen = std::sqrt(cb.r * cb.r + cb.g * cb.g + cb.b * cb.b);
            const float maxResidual = std::max(12.0f, 0.1f * cbLen);

            // The sprite colour this pixel is blended with is the one right behind
            // it, so take the nearest ring of deeper pixels that explains it.
            float bestA = 2.0f;
            Rgb bestF{};
            bool hasDeeper = false;
            const int fgRadius = 4;
            for (int r = 1; r <= fgRadius && bestA > 1.5f; ++r) {
                float ringA = 2.0f;
                for (int oy = -r; oy <= r; ++oy) {
                    for (int ox = -r; ox <= r; ++ox) {
                        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                        int nx = cx + ox;
                        int ny = cy + oy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        int n = ny * w + nx;
                        if (!remaining(n) || layer[n] <= l) continue;
                        hasDeeper = true;

                        Rgb F = ColorAt(px, n);
                        Rgb fb{ F.r - B.r, F.g - B.g, F.b - B.b };
                        float fbLenSq = fb.r * fb.r + fb.g * fb.g + fb.b * fb.b;
                        if (fbLenSq < 48.0f * 48.0f) continue;

                        float a = (cb.r * fb.r + cb.g * fb.g + cb.b * fb.b) / fbLenSq;
                        if (a < 0.0f || a > 1.05f || a >= ringA) continue;

                        Rgb res{ cb.r - a * fb.r, cb.g - a * fb.g, cb.b - a * fb.b };
                        if (res.r * res.r + res.g * res.g + res.b * res.b > maxResidual * maxResidual) continue;

                        ringA = a;
                        bestF = F;
                    }
                }
                bestA = ringA;
            }

            if (bestA > 1.5f) {
                // Not a blend of the sprite behind it, so it is a colour of its
                // own. With no sprite behind it at all (thin leftovers) it only
                // goes when it is plainly still background.
                if (!hasDeeper && modelDist[i] <= kAtTolerance * 3 / 2) coverage[i] = 0;
                continue;
            }

            float a = std::min(bestA, 1.0f);
            if (l > 1 && a <= outerCoverage) continue;
            if (a > 0.9f) continue;
            if (a < 0.1f) {
                coverage[i] = 0;
                continue;
            }

            coverage[i] = static_cast<uint8_t>(a * 255.0f + 0.5f);
            size_t idx = static_cast<size_t>(i) * 4;
            if (a >= 0.35f) {
                result[idx] = ToByte(B.r + cb.r / a);
                result[idx + 1] = ToByte(B.g + cb.g / a);
                result[idx + 2] = ToByte(B.b + cb.b / a);
            } else {
                result[idx] = ToByte(bestF.r);
                result[idx + 1] = ToByte(bestF.g);
                result[idx + 2] = ToByte(bestF.b);
            }
            result[idx + 3] = static_cast<uint8_t>(std::max(1.0f, px[idx + 3] * a + 0.5f));
        }
    }

    for (int l = 1; l <= maxLayer; ++l) {
        for (int i : layers[l]) {
            if (coverage[i] != 0) continue;
            size_t idx = static_cast<size_t>(i) * 4;
            result[idx] = 0;
            result[idx + 1] = 0;
            result[idx + 2] = 0;
            result[idx + 3] = 0;
        }
    }

    return result;
}

}
