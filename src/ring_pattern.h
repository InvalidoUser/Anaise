// Anaise -- the luminous ring pattern that hangs behind the working screens.
//
// The shapes follow the orthogonal ring patterns in the plane: every circle of
// a ring crosses its two neighbours at right angles, and each ring inward
// crosses the ring before it at right angles too. Straight chords join the
// circle centres, as in the paper's figures. The whole thing turns very
// slowly, the rings counter-rotating and breathing, and an arm of light
// travels around it.
//
// Everything is accumulated as a field of light and then ordered-dithered down
// to pure black and white, so the glow stays inside the app's two-colour
// palette instead of introducing greys.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace anaise {

class RingBackground {
public:
    RingBackground() = default;
    RingBackground(const RingBackground&) = delete;
    RingBackground& operator=(const RingBackground&) = delete;
    ~RingBackground() { release(); }

    // Draws the pattern over the whole of `target`, which must already be
    // filled with black. The pattern fades out across `topFade` and
    // `bottomFade` device pixels so titles and footnotes keep a clean field,
    // and dims between `quietTop` and `quietBottom` so a menu can sit in it.
    void draw(HDC target, int width, int height, ULONGLONG nowMs,
              int topFade = 0, int bottomFade = 0, int quietTop = 0,
              int quietBottom = 0) {
        if (width <= 0 || height <= 0) return;
        if (!ensureSurface(width, height)) return;

        topFade_ = static_cast<float>(topFade) / static_cast<float>(scale_);
        bottomFade_ =
            static_cast<float>(bottomFade) / static_cast<float>(scale_);
        quietTop_ = static_cast<float>(quietTop) / static_cast<float>(scale_);
        quietBottom_ =
            static_cast<float>(quietBottom) / static_cast<float>(scale_);

        std::memset(field_.data(), 0, field_.size() * sizeof(float));
        buildGeometry(fieldWidth_, fieldHeight_,
                      static_cast<double>(nowMs) * 0.001);
        paintField();
        dither();

        if (fieldWidth_ == width && fieldHeight_ == height) {
            BitBlt(target, 0, 0, width, height, surfaceDc_, 0, 0, SRCCOPY);
        } else {
            SetStretchBltMode(target, COLORONCOLOR);
            StretchBlt(target, 0, 0, width, height, surfaceDc_, 0, 0,
                       fieldWidth_, fieldHeight_, SRCCOPY);
        }
    }

private:
    struct Circle {
        float x;
        float y;
        float radius;
        float weight;
    };

    struct Chord {
        float x0;
        float y0;
        float x1;
        float y1;
        float weight;
    };

    struct Node {
        double x;
        double y;
        double light;
    };

    static constexpr double kPi = 3.14159265358979323846;
    // How far the light of a line reaches, in device pixels.
    static constexpr float kGlowPixels = 12.0f;
    // The art is rendered at native size until it gets expensive, then at a
    // lower resolution and stretched, which keeps fullscreen cheap.
    static constexpr long long kMaxFieldPixels = 2400000;

    // ------------------------------------------------------------- surface
    bool ensureSurface(int width, int height) {
        int scale = 1;
        const long long pixels =
            static_cast<long long>(width) * static_cast<long long>(height);
        while (pixels / (static_cast<long long>(scale) * scale) >
                   kMaxFieldPixels &&
               scale < 4) {
            ++scale;
        }
        const int fieldWidth = std::max(1, width / scale);
        const int fieldHeight = std::max(1, height / scale);
        if (surfaceDc_ && fieldWidth == fieldWidth_ &&
            fieldHeight == fieldHeight_) {
            return true;
        }

        release();
        fieldWidth_ = fieldWidth;
        fieldHeight_ = fieldHeight;
        scale_ = scale;
        glow_ = kGlowPixels / static_cast<float>(scale_);

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = fieldWidth_;
        info.bmiHeader.biHeight = -fieldHeight_;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                   nullptr, 0);
        if (!bitmap_ || !bits) {
            release();
            return false;
        }
        pixels_ = static_cast<std::uint32_t*>(bits);
        surfaceDc_ = CreateCompatibleDC(nullptr);
        if (!surfaceDc_) {
            release();
            return false;
        }
        previousBitmap_ = SelectObject(surfaceDc_, bitmap_);
        field_.assign(static_cast<std::size_t>(fieldWidth_) * fieldHeight_,
                      0.0f);
        circles_.reserve(160);
        chords_.reserve(160);
        return true;
    }

    void release() {
        field_.clear();
        circles_.clear();
        chords_.clear();
        if (surfaceDc_) {
            if (previousBitmap_) SelectObject(surfaceDc_, previousBitmap_);
            DeleteDC(surfaceDc_);
        }
        if (bitmap_) DeleteObject(bitmap_);
        surfaceDc_ = nullptr;
        bitmap_ = nullptr;
        previousBitmap_ = nullptr;
        pixels_ = nullptr;
        fieldWidth_ = 0;
        fieldHeight_ = 0;
        scale_ = 1;
    }

    // ------------------------------------------------------------ geometry
    // The light travels around the pattern once every ~18 seconds.
    static double sweep(double angle, double time) {
        const double turn = 0.5 + 0.5 * std::cos(angle - time * 0.35);
        return 0.42 + 0.58 * turn * turn;
    }

    void buildGeometry(int width, int height, double time) {
        circles_.clear();
        chords_.clear();

        const double minSide = std::min(width, height);
        const double cx =
            width * 0.5 + std::sin(time * 0.11) * minSide * 0.014;
        const double cy =
            height * 0.52 + std::cos(time * 0.13) * minSide * 0.011;
        const double breath = 1.0 + 0.024 * std::sin(time * 0.31);

        // How many circles make each ring, outermost first. The counts grow
        // inward so the heart of the pattern turns into fine white dust.
        static constexpr int kRingCounts[] = {8, 9, 11, 13, 16, 20, 24, 28};
        constexpr int kRingTotal =
            static_cast<int>(std::size(kRingCounts));

        // The whole pattern reaches kEnvelope of the short side, including the
        // outermost circles rather than just their centres.
        const double outerSpread = std::sqrt(2.0) *
                                   std::sin(kPi / kRingCounts[0]);
        const double envelope = minSide * 0.60 * breath;
        double distance = envelope / (1.0 + outerSpread);
        double phase = 0.0;

        for (int ring = 0; ring < kRingTotal; ++ring) {
            const int count = kRingCounts[ring];
            const double spread = std::sqrt(2.0) * std::sin(kPi / count);
            const double radius = distance * spread;
            if (radius < 1.0) break;

            const double direction = (ring % 2 == 0) ? 1.0 : -1.0;
            const double spin =
                time * direction * 0.42 / (1.0 + 0.18 * ring);
            const double ringPhase = phase + spin;
            const double ringWeight = 0.44 + 0.56 / (1.0 + 0.55 * ring);
            // The chords are quieter than the circles they join.
            const double chordWeight = ringWeight * 0.42;

            nodes_.clear();
            for (int i = 0; i < count; ++i) {
                const double angle = ringPhase + 2.0 * kPi * i / count;
                const double x = cx + distance * std::cos(angle);
                const double y = cy + distance * std::sin(angle);
                const double light = sweep(angle, time);
                nodes_.push_back(Node{x, y, light});
                circles_.push_back(Circle{
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(radius),
                    static_cast<float>(ringWeight * light)});
            }
            for (int i = 0; i < count; ++i) {
                const Node& a = nodes_[i];
                const Node& b = nodes_[(i + 1) % count];
                chords_.push_back(Chord{
                    static_cast<float>(a.x), static_cast<float>(a.y),
                    static_cast<float>(b.x), static_cast<float>(b.y),
                    static_cast<float>(chordWeight * 0.5 *
                                       (a.light + b.light))});
            }

            // Step inward so the next ring crosses this one at right angles.
            const int next = std::min(ring + 1, kRingTotal - 1);
            const double a = kPi / count;
            const double b = kPi / kRingCounts[next];
            const double cosA = std::cos(a);
            const double cos2A = std::cos(2.0 * a);
            const double cos2B = std::cos(2.0 * b);
            const double root = cosA * cosA - cos2A * cos2B;
            const double factor =
                root > 0.0 ? (cosA - std::sqrt(root)) / cos2B : 0.5;
            // Half a step, so the circles of the next ring sit between ours.
            phase += kPi / count;
            distance *= std::clamp(factor, 0.3, 0.95);
            if (distance < 1.0) break;
        }
        nodes_.clear();
    }

    // ------------------------------------------------------------- drawing
    // Light around a line of given distance, in device pixels: a crisp core
    // and two progressively wider haloes, faded to nothing at kGlowPixels.
    static const std::array<float, 121>& profile() {
        static const std::array<float, 121> table = [] {
            std::array<float, 121> result{};
            constexpr float extent = 12.0f;
            for (int i = 0; i <= 120; ++i) {
                const float d = extent * static_cast<float>(i) / 120.0f;
                const float core = std::exp(-(d * d) / (2.0f * 0.62f * 0.62f));
                const float inner = std::exp(-(d * d) / (2.0f * 2.8f * 2.8f));
                const float outer = std::exp(-(d * d) / (2.0f * 7.0f * 7.0f));
                const float window =
                    std::clamp((extent - d) / 4.0f, 0.0f, 1.0f);
                result[i] = (core + inner * 0.34f + outer * 0.13f) * window;
            }
            return result;
        }();
        return table;
    }

    float lightAt(float distance) const {
        const std::array<float, 121>& table = profile();
        // The profile is defined in device pixels, so at a reduced render
        // resolution the same halo still covers the same screen area.
        const float step = 120.0f / kGlowPixels * static_cast<float>(scale_);
        int index = static_cast<int>(distance * step);
        if (index < 0) return 0.0f;
        if (index > 120) return 0.0f;
        return table[index];
    }

    void paintField() {
        for (const Circle& circle : circles_) addCircle(circle);
        for (const Chord& chord : chords_) addChord(chord);
    }

    void addCircle(const Circle& circle) {
        const float outer = circle.radius + glow_;
        const float inner = std::max(0.0f, circle.radius - glow_);
        const int firstRow =
            std::max(0, static_cast<int>(std::floor(circle.y - outer)));
        const int lastRow = std::min(
            fieldHeight_ - 1, static_cast<int>(std::ceil(circle.y + outer)));
        if (firstRow > lastRow) return;

        const float outer2 = outer * outer;
        const float inner2 = inner * inner;
        float* const data = field_.data();

        for (int row = firstRow; row <= lastRow; ++row) {
            const float dy = (static_cast<float>(row) + 0.5f) - circle.y;
            const float dy2 = dy * dy;
            const float far2 = outer2 - dy2;
            if (far2 <= 0.0f) continue;
            const float farX = std::sqrt(far2);
            const float near2 = inner2 - dy2;
            const float nearX = near2 > 0.0f ? std::sqrt(near2) : 0.0f;

            float* const line = data + static_cast<std::size_t>(row) * fieldWidth_;
            const int leftOuter =
                std::max(0, static_cast<int>(std::floor(circle.x - farX)));
            const int leftInner = std::min(
                fieldWidth_ - 1,
                static_cast<int>(std::ceil(circle.x - nearX)));
            for (int x = leftOuter; x <= leftInner; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) - circle.x;
                const float distance =
                    std::fabs(std::sqrt(dx * dx + dy2) - circle.radius);
                line[x] += lightAt(distance) * circle.weight;
            }

            const int rightInner = std::max(
                0, static_cast<int>(std::floor(circle.x + nearX)));
            const int rightOuter = std::min(
                fieldWidth_ - 1,
                static_cast<int>(std::ceil(circle.x + farX)));
            for (int x = rightInner; x <= rightOuter; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) - circle.x;
                const float distance =
                    std::fabs(std::sqrt(dx * dx + dy2) - circle.radius);
                line[x] += lightAt(distance) * circle.weight;
            }
        }
    }

    void addChord(const Chord& chord) {
        const float dx = chord.x1 - chord.x0;
        const float dy = chord.y1 - chord.y0;
        const float length2 = dx * dx + dy * dy;
        if (length2 < 0.0001f) return;
        const int firstRow = std::max(
            0, static_cast<int>(std::floor(
                   std::min(chord.y0, chord.y1) - glow_)));
        const int lastRow = std::min(
            fieldHeight_ - 1,
            static_cast<int>(std::ceil(std::max(chord.y0, chord.y1) + glow_)));
        const int firstCol = std::max(
            0, static_cast<int>(std::floor(
                   std::min(chord.x0, chord.x1) - glow_)));
        const int lastCol = std::min(
            fieldWidth_ - 1,
            static_cast<int>(std::ceil(std::max(chord.x0, chord.x1) + glow_)));
        float* const data = field_.data();

        for (int row = firstRow; row <= lastRow; ++row) {
            const float py = (static_cast<float>(row) + 0.5f) - chord.y0;
            float* const line = data + static_cast<std::size_t>(row) * fieldWidth_;
            for (int col = firstCol; col <= lastCol; ++col) {
                const float px = (static_cast<float>(col) + 0.5f) - chord.x0;
                const float t =
                    std::clamp((px * dx + py * dy) / length2, 0.0f, 1.0f);
                const float ddx = px - dx * t;
                const float ddy = py - dy * t;
                const float distance = std::sqrt(ddx * ddx + ddy * ddy);
                if (distance > glow_) continue;
                line[col] += lightAt(distance) * chord.weight;
            }
        }
    }

    // -------------------------------------------------------------- dither
    void dither() {
        static constexpr int kBayer[8][8] = {
            { 0, 32,  8, 40,  2, 34, 10, 42},
            {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44,  4, 36, 14, 46,  6, 38},
            {60, 28, 52, 20, 62, 30, 54, 22},
            { 3, 35, 11, 43,  1, 33,  9, 41},
            {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47,  7, 39, 13, 45,  5, 37},
            {63, 31, 55, 23, 61, 29, 53, 21},
        };
        const float* source = field_.data();
        std::uint32_t* target = pixels_;
        for (int row = 0; row < fieldHeight_; ++row) {
            const int* thresholds = kBayer[row & 7];
            float gain = 1.0f;
            if (topFade_ > 0.0f) {
                const float t = static_cast<float>(row) / topFade_;
                gain *= std::clamp((t - 0.4f) / 0.6f, 0.0f, 1.0f);
            }
            if (bottomFade_ > 0.0f) {
                const float t =
                    static_cast<float>(fieldHeight_ - 1 - row) / bottomFade_;
                gain *= std::clamp((t - 0.35f) / 0.65f, 0.0f, 1.0f);
            }
            if (quietBottom_ > quietTop_) {
                // A soft, dimmed band for the menu to sit in.
                constexpr float kEdge = 22.0f;
                const float y = static_cast<float>(row);
                float outside = 0.0f;
                if (y < quietTop_) {
                    outside = (quietTop_ - y) / kEdge;
                } else if (y > quietBottom_) {
                    outside = (y - quietBottom_) / kEdge;
                }
                gain *= 0.30f + 0.70f * std::clamp(outside, 0.0f, 1.0f);
            }
            const float* line = source + static_cast<std::size_t>(row) * fieldWidth_;
            std::uint32_t* out = target + static_cast<std::size_t>(row) * fieldWidth_;
            for (int col = 0; col < fieldWidth_; ++col) {
                const float level = line[col] * gain;
                out[col] = level * 64.0f > static_cast<float>(thresholds[col & 7])
                               ? 0xFFFFFFFFu
                               : 0xFF000000u;
            }
        }
    }

    // ------------------------------------------------------------- members
    HDC surfaceDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
    std::uint32_t* pixels_ = nullptr;
    int fieldWidth_ = 0;
    int fieldHeight_ = 0;
    int scale_ = 1;
    float glow_ = kGlowPixels;
    float topFade_ = 0.0f;
    float bottomFade_ = 0.0f;
    float quietTop_ = 0.0f;
    float quietBottom_ = 0.0f;

    std::vector<float> field_;
    std::vector<Circle> circles_;
    std::vector<Chord> chords_;
    std::vector<Node> nodes_;
};

}  // namespace anaise
