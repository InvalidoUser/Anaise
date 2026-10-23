#pragma once

// TENTACLE: a black-and-white swarm defence. A living mouth sits at the end of
// a walled maze and lasers whatever crawls around the corners towards it. The
// player buys small mouths (units), drags them into position and evolves them
// while thousands of procedural blobs flow through the corridors on a shared
// flow field. The simulation and the pixel renderer are platform neutral so the
// same code runs live, in headless captures and in the self-test.

#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace anaise {

class Tentacle {
public:
    static constexpr int MW = 64, MH = 40;
    static constexpr int kWaves = 25;
    static constexpr int kMaxBlobs = 6000;
    static constexpr int kButtons = 10;
    // Sound cues, drained by the host once per frame.
    enum Sound : unsigned {
        SLaser = 1u, SSplat = 2u, SBuy = 4u, SWave = 8u, SHurt = 16u, SEvolve = 32u, SPulse = 64u,
    };
    enum class State { Title, Build, Wave, Lost, Won };
    enum Kind { Mite, Puddle, Splitter, Shell, Jelly, Brute, KindCount };
    enum UnitKind { Biter, Stalk, Maw, Digester, UnitKindCount };

    Tentacle() { buildMap(); reset(); }

    // ------------------------------------------------------------ lifecycle
    void reset() {
        blobs_.clear(); units_.clear(); particles_.clear(); beams_.clear(); rings_.clear();
        spawnQueue_.clear(); pendingMites_.clear();
        stain_.assign(size_t(MW * 4) * (MH * 4), 0.f);
        state_ = State::Title; paused_ = false; endless_ = false;
        wave_ = 0; biomass_ = 160; kills_ = 0; lost_ = 0;
        baseHp_ = baseMax_ = 100; mouthLevel_ = 1; colony_ = 0;
        pulseCd_ = 0; intermission_ = 0; spawnCarry_ = 0; spawnRate_ = 0;
        time_ = 0; clock_ = 0; hurt_ = 0; shake_ = 0; nextId_ = 1; rng_ = 0x9e3779b9u;
        mouthTargets_.fill(-1);
        dragging_ = false; hover_ = -1; toast_.clear(); toastTime_ = 0; sounds_ = 0;
        soundGap_.fill(0); mark_ = 0;
    }

    void begin() { if (state_ == State::Title) { state_ = State::Build; intermission_ = 20; toast("BUY A MOUTH. DRAG IT TO A CORNER."); } }

    // ------------------------------------------------------------ input
    void key(int k) {
        if (k == 'P' && state_ != State::Title) { paused_ = !paused_; return; }
        if (k == 'R' && (state_ == State::Lost || state_ == State::Won || paused_)) { reset(); begin(); return; }
        if (state_ == State::Title) { if (k == kSpace || k == kReturn) begin(); return; }
        if (state_ == State::Lost) return;
        if (state_ == State::Won) {
            if (k == kSpace || k == kReturn) { endless_ = true; state_ = State::Build; intermission_ = 10; toast("ENDLESS. THE TIDE RETURNS."); }
            return;
        }
        if (paused_) return;
        if (k >= '1' && k <= '4') press(k - '1');
        else if (k == 'E') press(4);
        else if (k == 'U') press(5);
        else if (k == 'C') press(6);
        else if (k == 'Q') press(7);
        else if (k == 'H') press(8);
        else if (k == kSpace || k == kReturn) press(9);
        else if (k == 'A') { for (auto& u : units_) u.selected = u.type != Digester; }
    }

    // Coordinates are in the content bitmap the host gave to draw().
    void mouseDown(int x, int y, bool left) {
        mouseX_ = x; mouseY_ = y;
        if (state_ == State::Title) { if (left) begin(); return; }
        if (state_ == State::Lost || state_ == State::Won || paused_) return;
        const Layout L = layout(lastW_, lastH_);
        const int b = buttonAt(L, x, y);
        if (b >= 0) { if (left) press(b); return; }
        float wx, wy;
        if (!toWorld(L, x, y, wx, wy)) return;
        if (left) { dragging_ = true; dragX0_ = x; dragY0_ = y; }
        else orderMove(wx, wy);
    }

    void mouseMove(int x, int y) {
        mouseX_ = x; mouseY_ = y;
        hover_ = buttonAt(layout(lastW_, lastH_), x, y);
    }

    void mouseUp() {
        if (!dragging_) return;
        dragging_ = false;
        const Layout L = layout(lastW_, lastH_);
        const bool additive = false;
        if (std::abs(mouseX_ - dragX0_) < 5 && std::abs(mouseY_ - dragY0_) < 5) {
            float wx, wy;
            if (!toWorld(L, mouseX_, mouseY_, wx, wy)) return;
            int best = -1; float bd = 1e9f;
            for (int i = 0; i < int(units_.size()); ++i) {
                const float dx = units_[i].x - wx, dy = units_[i].y - wy, d = dx * dx + dy * dy;
                const float r = ut(units_[i].type).radius + .25f;
                if (d < r * r && d < bd) { bd = d; best = i; }
            }
            if (!additive) for (auto& u : units_) u.selected = false;
            if (best >= 0) units_[best].selected = true;
            return;
        }
        float ax, ay, bx, by;
        toWorldClamped(L, std::min(dragX0_, mouseX_), std::min(dragY0_, mouseY_), ax, ay);
        toWorldClamped(L, std::max(dragX0_, mouseX_), std::max(dragY0_, mouseY_), bx, by);
        for (auto& u : units_) u.selected = u.x >= ax && u.x <= bx && u.y >= ay && u.y <= by;
    }

    bool dragging() const { return dragging_; }
    bool paused() const { return paused_; }
    bool title() const { return state_ == State::Title; }
    bool fighting() const { return state_ == State::Wave; }
    unsigned takeSounds() { const unsigned s = sounds_; sounds_ = 0; return s; }
    int blobCount() const { return int(blobs_.size()); }
    int wave() const { return wave_; }

    // ------------------------------------------------------------ simulation
    void update(std::uint64_t now, bool focused) {
        const std::uint64_t elapsed = clock_ && now >= clock_ ? std::min<std::uint64_t>(now - clock_, 100) : 0;
        clock_ = now;
        if (!focused || paused_ || state_ == State::Title) return;
        double dt = elapsed / 1000.0;
        while (dt > 1e-6) { const double step = std::min(dt, 0.025); simulate(float(step)); dt -= step; }
    }

    void simulate(float dt) {
        time_ += dt;
        hurt_ = std::max(0.f, hurt_ - dt); shake_ = std::max(0.f, shake_ - dt * 3);
        toastTime_ = std::max(0.f, toastTime_ - dt);
        for (auto& g : soundGap_) g = std::max(0.f, g - dt);
        pulseCd_ = std::max(0.f, pulseCd_ - dt);
        if (state_ == State::Lost || state_ == State::Won) { stepParticles(dt); return; }
        if (state_ == State::Build) {
            intermission_ -= dt;
            if (intermission_ <= 0) startWave();
        }
        if (state_ == State::Wave) spawnStep(dt);
        for (const auto& u : units_) if (u.type == Digester) biomass_ += digestRate(u) * dt;
        if (state_ == State::Wave) biomass_ += 0.6f * dt;
        buildHash();
        stepBlobs(dt);
        stepUnits(dt);
        stepMouth(dt);
        compact();
        stepParticles(dt);
        for (auto& s : stain_) if (s > 0) s = std::max(0.f, s - dt * .004f);
        if (state_ == State::Wave && spawnQueue_.empty() && blobs_.empty()) waveCleared();
        if (baseHp_ <= 0 && state_ != State::Lost) {
            baseHp_ = 0; state_ = State::Lost; dragging_ = false;
            for (int i = 0; i < 160; ++i) particle(base_[0], base_[1], 9.f);
            sounds_ |= SHurt | SPulse;
        }
    }

    // A deterministic mid-game battle for previews and captures.
    void preview(std::uint64_t age) {
        reset(); begin(); biomass_ = 640; colony_ = 2; mouthLevel_ = 3; wave_ = 17;
        addUnit(Biter, 39.5f, 19.f, 3); addUnit(Biter, 39.5f, 21.5f, 2); addUnit(Maw, 40.5f, 20.2f, 2);
        addUnit(Stalk, 49.5f, 18.5f, 2); addUnit(Stalk, 49.5f, 22.5f, 1); addUnit(Biter, 44.5f, 4.5f, 2);
        addUnit(Biter, 44.5f, 35.f, 2); addUnit(Digester, 60.f, 33.f, 2);
        units_[0].selected = units_[1].selected = units_[2].selected = true;
        startWave();
        const int steps = 380 + int(age % 4000) / 25;
        for (int i = 0; i < steps; ++i) simulate(0.025f);
        biomass_ = std::max(biomass_, 300.f);
        hover_ = 1;
    }

    // Headless verification of the rules. Returns false on the first failure.
    static bool selfTest() {
        Tentacle t;
        // Every spawn hole must reach the mouth.
        for (const auto& s : t.spawns_) if (t.blobDist_[t.tile(s[0], s[1])] >= kInf) return false;
        t.begin();
        const float before = t.biomass_;
        t.press(0);
        if (t.units_.size() != 1 || t.biomass_ != before - ut(Biter).cost) return false;
        // The new mouth walks round the long wall to the far chamber.
        t.orderMove(39.f, 12.f);
        for (int i = 0; i < 1200 && t.units_[0].moving; ++i) t.simulate(0.025f);
        if (t.units_[0].moving || std::hypot(t.units_[0].x - 39.f, t.units_[0].y - 12.f) > .6f) return false;
        // Evolution costs biomass and raises the level.
        t.biomass_ = 500; t.units_[0].selected = true; t.press(4);
        if (t.units_[0].level != 2 || t.biomass_ >= 500) return false;
        // The first wave dies to the defence and pays out.
        const float bank = t.biomass_;
        t.startWave();
        for (int i = 0; i < 4800 && t.state_ == State::Wave; ++i) t.simulate(0.025f);
        if (t.state_ != State::Build || t.biomass_ <= bank || t.kills_ == 0) return false;
        // Splitters burst into mites; puddles that meet become one.
        t.blobs_.clear();
        t.spawnBlob(Splitter, 20.5f, 30.5f);
        t.buildHash(); t.damageBlob(0, 1e6f); t.compact();
        if (t.blobs_.size() != 3) return false;
        for (const auto& b : t.blobs_) if (b.kind != Mite) return false;
        t.blobs_.clear();
        t.spawnBlob(Puddle, 20.5f, 30.5f); t.spawnBlob(Puddle, 20.52f, 30.5f);
        t.buildHash(); t.stepBlobs(0.02f); t.compact();
        if (t.blobs_.size() != 1 || t.blobs_[0].r <= sp(Puddle).r) return false;
        // Thousands of blobs flow without tunnelling into the walls.
        t.blobs_.clear(); t.units_.clear(); t.baseHp_ = 1e9f;
        for (int i = 0; i < 3000; ++i) {
            const auto& s = t.spawns_[i % 3];
            t.spawnBlob(Kind(i % 5), s[0] + t.frand() * 1.5f - .75f, s[1] + t.frand() * 1.5f - .75f);
        }
        t.state_ = State::Wave;
        for (int i = 0; i < 120; ++i) t.simulate(0.025f);
        int inside = 0;
        for (const auto& b : t.blobs_) {
            if (!std::isfinite(b.x) || !std::isfinite(b.y)) return false;
            if (t.solid(int(b.x), int(b.y))) ++inside;
        }
        if (inside > int(t.blobs_.size()) / 100 || t.blobs_.size() < 2500) return false;
        // The mouth falls when enough reaches it.
        Tentacle d; d.begin(); d.baseHp_ = 2; d.state_ = State::Wave;
        for (int i = 0; i < 40; ++i) d.spawnBlob(Mite, d.base_[0] - 2.5f, d.base_[1] + (i % 5) * .2f);
        for (int i = 0; i < 400 && d.state_ != State::Lost; ++i) d.simulate(0.025f);
        if (d.state_ != State::Lost) return false;
        // Rendering fills a frame without touching memory outside it.
        std::vector<std::uint32_t> frame(700 * 480 + 1, 0xabcdef12u);
        t.render(frame.data(), 700, 480, 1000);
        if (frame.back() != 0xabcdef12u) return false;
        return true;
    }

    // ------------------------------------------------------------ drawing
#ifdef _WIN32
    void draw(HDC dc, int width, int height, ULONGLONG now) {
        if (width <= 0 || height <= 0) return;
        frame_.resize(size_t(width) * height);
        render(frame_.data(), width, height, now);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, 0, 0, width, height, 0, 0, 0, height, frame_.data(), &info, DIB_RGB_COLORS);
    }
#endif

    void render(std::uint32_t* pixels, int w, int h, std::uint64_t now) {
        lastW_ = w; lastH_ = h;
        Canvas c{pixels, w, h};
        std::fill(pixels, pixels + size_t(w) * h, 0u);
        const Layout L = layout(w, h);
        const double clockSec = now / 1000.0;
        drawMap(c, L);
        drawSpawns(c, L);
        for (const auto& u : units_) drawUnitLegs(c, L, u);
        drawBase(c, L);
        for (const auto& b : blobs_) drawBlob(c, L, b);
        for (const auto& r : rings_) c.ring(L.ox + r.x * L.s, L.oy + r.y * L.s, r.r * L.s, 2.f);
        for (const auto& b : beams_) drawBeam(c, L, b);
        for (const auto& u : units_) drawUnit(c, L, u);
        for (const auto& p : particles_) {
            const int x = int(L.ox + p.x * L.s), y = int(L.oy + p.y * L.s);
            c.set(x, y); if (p.life > .35f) c.set(x + 1, y);
        }
        if (dragging_) c.dashed(std::min(dragX0_, mouseX_), std::min(dragY0_, mouseY_),
                                std::max(dragX0_, mouseX_), std::max(dragY0_, mouseY_), 3);
        drawHud(c, L);
        drawShop(c, L);
        drawOverlay(c, L, clockSec);
    }

private:
    static constexpr int kSpace = 0x20, kReturn = 0x0D;
    static constexpr float kInf = 1e9f;

    // ---------------------------------------------------------------- data
    struct Species { const char* name; float r, speed, hp; float reward, armor, mass, accel, bite, touch; int cost; };
    static const Species& sp(int k) {
        static const Species table[KindCount] = {
            // name       radius speed hp   reward armor mass accel bite touch cost
            {"MITE",      .20f, 3.1f,   5,  1.0f, 1.0f, 1.0f, 9.f, 1, 5, 1},
            {"PUDDLE",    .24f, 2.3f,   9,  1.2f, 1.0f, .7f,  5.f, 1, 4, 1},
            {"SPLITTER",  .32f, 2.0f,  26,  3.0f, 1.0f, 1.4f, 5.f, 2, 7, 4},
            {"SHELL",     .36f, 1.35f, 58,  5.0f, .45f, 4.0f, 2.5f, 4, 10, 7},
            {"JELLY",     .40f, 1.6f,  40,  7.0f, .85f, 1.5f, 4.f, 3, 8, 9},
            {"BRUTE",     .85f, .95f, 760, 90.0f, .70f, 30.f, 1.5f, 22, 45, 0},
        };
        return table[k];
    }
    struct UnitType { const char* name; int cost; float range, damage, interval, speed, hp, radius; const char* blurb; };
    static const UnitType& ut(int t) {
        static const UnitType table[UnitKindCount] = {
            {"BITER", 40, 4.2f, 22, 0, 4.0f, 60, .42f, "BITER  CHEAP. A STEADY BEAM AT SHORT RANGE."},
            {"STALK", 90, 9.5f, 78, .9f, 3.0f, 45, .42f, "STALK  AN EYE. LONG BEAM PIERCES SIX IN A LINE."},
            {"MAW", 140, 3.4f, 36, 1.1f, 2.6f, 130, .56f, "MAW  BLOCKS A CORRIDOR AND BITES ALL AROUND IT."},
            {"DIGEST", 120, 0, 0, 0, 1.6f, 70, .50f, "DIGESTER  NO TEETH. TURNS TIME INTO BIOMASS."},
        };
        return table[t];
    }

    struct Blob {
        float x, y, vx, vy, hp, maxhp, r, phase, buff, flash;
        std::uint32_t id; std::uint8_t kind; bool dead, rewarded;
    };
    struct Unit {
        float x, y, hp, maxhp, cooldown, face, tx, ty, hurt, anim;
        int type, level, target; float retarget;
        bool selected, moving;
        std::vector<float> path;
    };
    struct Particle { float x, y, vx, vy, life; };
    struct Beam { float x0, y0, x1, y1, life, width; };
    struct Ring { float x, y, r, grow, life; };
    struct Pending { float x, y; int kind; };

    // ---------------------------------------------------------------- map
    std::array<std::uint8_t, MW * MH> wall_{};
    std::vector<float> blobDist_, dirX_, dirY_;
    std::array<std::array<float, 2>, 5> spawns_{};
    std::array<float, 2> base_{{57.5f, 20.f}};

    static int tile(float x, float y) { return int(y) * MW + int(x); }
    bool solid(int x, int y) const { return x < 0 || y < 0 || x >= MW || y >= MH || wall_[y * MW + x]; }

    void buildMap() {
        wall_.fill(0);
        auto box = [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) wall_[y * MW + x] = 1;
        };
        box(0, 0, MW - 1, 0); box(0, MH - 1, MW - 1, MH - 1); box(0, 0, 0, MH - 1); box(MW - 1, 0, MW - 1, MH - 1);
        // Four baffles turn the run into a serpent of corners.
        box(12, 0, 13, 25);
        box(24, 14, 25, 39);
        box(36, 0, 37, 16); box(36, 24, 37, 39);
        box(46, 8, 47, 31);
        const int pillars[][2] = {{6, 11}, {6, 27}, {18, 7}, {18, 20}, {30, 6}, {30, 30}, {42, 4}, {42, 34}, {52, 10}, {52, 28}, {41, 19}};
        for (const auto& p : pillars) box(p[0], p[1], p[0] + 1, p[1] + 1);
        spawns_ = {{{2.5f, 4.5f}, {2.5f, 20.5f}, {2.5f, 35.5f}, {30.5f, 2.5f}, {30.5f, 37.5f}}};
        computeField(blobDist_, base_[0], base_[1], 2.1f);
        computeDirs();
    }

    void computeField(std::vector<float>& d, float tx, float ty, float radius) const {
        d.assign(MW * MH, kInf);
        using Node = std::pair<float, int>;
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> q;
        for (int y = 0; y < MH; ++y) for (int x = 0; x < MW; ++x) {
            if (solid(x, y)) continue;
            const float dx = x + .5f - tx, dy = y + .5f - ty;
            if (dx * dx + dy * dy <= radius * radius) { d[y * MW + x] = 0; q.push({0.f, y * MW + x}); }
        }
        if (q.empty()) {
            const int x = std::clamp(int(tx), 0, MW - 1), y = std::clamp(int(ty), 0, MH - 1);
            if (solid(x, y)) return;
            d[y * MW + x] = 0; q.push({0.f, y * MW + x});
        }
        while (!q.empty()) {
            const auto [cd, i] = q.top(); q.pop();
            if (cd > d[i]) continue;
            const int x = i % MW, y = i / MW;
            for (int k = 0; k < 8; ++k) {
                const int X = x + kNx[k], Y = y + kNy[k];
                if (solid(X, Y) || (k >= 4 && (solid(X, y) || solid(x, Y)))) continue;
                const float nd = cd + kNc[k];
                const int j = Y * MW + X;
                if (nd < d[j]) { d[j] = nd; q.push({nd, j}); }
            }
        }
    }

    void computeDirs() {
        dirX_.assign(MW * MH, 0.f); dirY_.assign(MW * MH, 0.f);
        for (int y = 0; y < MH; ++y) for (int x = 0; x < MW; ++x) {
            const int i = y * MW + x;
            if (solid(x, y) || blobDist_[i] >= kInf) continue;
            float gx = 0, gy = 0;
            for (int k = 0; k < 8; ++k) {
                const int X = x + kNx[k], Y = y + kNy[k];
                if (solid(X, Y) || (k >= 4 && (solid(X, y) || solid(x, Y)))) continue;
                const float drop = blobDist_[i] - blobDist_[Y * MW + X];
                if (drop > 0) { gx += kNx[k] / kNc[k] * drop; gy += kNy[k] / kNc[k] * drop; }
            }
            if (gx == 0 && gy == 0) { gx = base_[0] - (x + .5f); gy = base_[1] - (y + .5f); }
            const float n = std::sqrt(gx * gx + gy * gy) + 1e-6f;
            dirX_[i] = gx / n; dirY_[i] = gy / n;
        }
    }

    // Bilinear steering between tile centres: corners are taken as curves.
    void sampleDir(float x, float y, float& ox, float& oy) const {
        const float fx = x - .5f, fy = y - .5f;
        const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
        const float ax = fx - x0, ay = fy - y0;
        float sx = 0, sy = 0, sw = 0;
        for (int k = 0; k < 4; ++k) {
            const int X = x0 + (k & 1), Y = y0 + (k >> 1);
            if (solid(X, Y)) continue;
            const int i = Y * MW + X;
            if (blobDist_[i] >= kInf) continue;
            const float w = ((k & 1) ? ax : 1 - ax) * ((k >> 1) ? ay : 1 - ay);
            sx += dirX_[i] * w; sy += dirY_[i] * w; sw += w;
        }
        if (sw < 1e-4f) { sx = base_[0] - x; sy = base_[1] - y; }
        const float n = std::sqrt(sx * sx + sy * sy) + 1e-6f;
        ox = sx / n; oy = sy / n;
    }

    void pushOutOfWalls(float& x, float& y, float r) const {
        const int cx = int(std::floor(x)), cy = int(std::floor(y));
        if (solid(cx, cy)) {
            // The centre is inside a wall: leave by the nearest open side.
            const float fx = x - cx, fy = y - cy;
            float best = 9; int dir = -1;
            const float dist[4] = {fx, 1 - fx, fy, 1 - fy};
            const int ox[4] = {-1, 1, 0, 0}, oy[4] = {0, 0, -1, 1};
            for (int k = 0; k < 4; ++k) if (!solid(cx + ox[k], cy + oy[k]) && dist[k] < best) { best = dist[k]; dir = k; }
            if (dir == 0) x = cx - r * .5f; else if (dir == 1) x = cx + 1 + r * .5f;
            else if (dir == 2) y = cy - r * .5f; else if (dir == 3) y = cy + 1 + r * .5f;
            else { x = base_[0]; y = base_[1]; }
        }
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            const int X = int(std::floor(x)) + dx, Y = int(std::floor(y)) + dy;
            if (!solid(X, Y)) continue;
            const float px = std::clamp(x, float(X), float(X + 1)), py = std::clamp(y, float(Y), float(Y + 1));
            const float ddx = x - px, ddy = y - py, d2 = ddx * ddx + ddy * ddy;
            if (d2 < r * r && d2 > 1e-10f) {
                const float d = std::sqrt(d2), push = r - d;
                x += ddx / d * push; y += ddy / d * push;
            }
        }
    }

    bool lineOfSight(float x0, float y0, float x1, float y1) const {
        int X = int(x0), Y = int(y0);
        const int X1 = int(x1), Y1 = int(y1);
        const float dx = x1 - x0, dy = y1 - y0;
        const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
        const float tdx = dx != 0 ? std::abs(1 / dx) : 1e9f, tdy = dy != 0 ? std::abs(1 / dy) : 1e9f;
        float tx = dx != 0 ? ((dx > 0 ? X + 1 - x0 : x0 - X) * tdx) : 1e9f;
        float ty = dy != 0 ? ((dy > 0 ? Y + 1 - y0 : y0 - Y) * tdy) : 1e9f;
        for (int n = 0; n < 128; ++n) {
            if (X == X1 && Y == Y1) return true;
            if (tx < ty) { X += sx; tx += tdx; } else { Y += sy; ty += tdy; }
            if (solid(X, Y)) return false;
        }
        return true;
    }

    // ---------------------------------------------------------------- state
    std::vector<Blob> blobs_;
    std::vector<Unit> units_;
    std::vector<Particle> particles_;
    std::vector<Beam> beams_;
    std::vector<Ring> rings_;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> spawnQueue_;
    std::vector<Pending> pendingMites_;
    std::vector<float> stain_, pushX_, pushY_;
    std::vector<int> cellHead_, cellNext_, remap_;
    std::vector<std::uint32_t> frame_;
    State state_ = State::Title;
    bool paused_ = false, endless_ = false;
    int wave_ = 0, kills_ = 0, lost_ = 0, mouthLevel_ = 1, colony_ = 0;
    float biomass_ = 0, baseHp_ = 100, baseMax_ = 100, pulseCd_ = 0, intermission_ = 0;
    float spawnCarry_ = 0, spawnRate_ = 0, time_ = 0, hurt_ = 0, shake_ = 0;
    std::uint64_t clock_ = 0;
    std::uint32_t nextId_ = 1, rng_ = 1;
    std::array<int, 3> mouthTargets_{};
    std::array<float, 8> soundGap_{};
    unsigned sounds_ = 0;
    std::string toast_; float toastTime_ = 0;
    bool dragging_ = false; int dragX0_ = 0, dragY0_ = 0, mouseX_ = 0, mouseY_ = 0, hover_ = -1;
    int lastW_ = 760, lastH_ = 520;
    float mark_ = 0; float markX_ = 0, markY_ = 0;

    static constexpr int kNx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static constexpr int kNy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static constexpr float kNc[8] = {1, 1, 1, 1, 1.41421f, 1.41421f, 1.41421f, 1.41421f};

    float frand() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return (rng_ & 0xffffff) / 16777216.f; }
    void toast(const std::string& s) { toast_ = s; toastTime_ = 3.5f; }
    void sound(unsigned bit, int slot, float gap) {
        if (soundGap_[slot] > 0) return;
        soundGap_[slot] = gap; sounds_ |= bit;
    }

    // ---------------------------------------------------------------- economy
    float colonyMul() const { return 1.f + .12f * colony_; }
    float rewardMul() const { return 1.f + .10f * colony_; }
    float mouthRange() const { return 6.5f + .7f * (mouthLevel_ - 1); }
    float mouthDps() const { return 24.f * std::pow(1.38f, float(mouthLevel_ - 1)) * colonyMul(); }
    int mouthBeams() const { return 1 + (mouthLevel_ >= 3) + (mouthLevel_ >= 5); }
    int mouthCost() const { return mouthLevel_ >= 7 ? 0 : int(110 * std::pow(1.75, mouthLevel_ - 1)); }
    int colonyCost() const { return int(200 * std::pow(1.65, colony_)); }
    static int evolveCost(const Unit& u) { return u.level >= 5 ? 0 : int(ut(u.type).cost * .8f * u.level); }
    float unitRange(const Unit& u) const { return ut(u.type).range + .45f * (u.level - 1); }
    float unitDamage(const Unit& u) const { return ut(u.type).damage * std::pow(1.35f, float(u.level - 1)) * colonyMul(); }
    static float digestRate(const Unit& u) { return 1.8f * (1 + .6f * (u.level - 1)); }
    int selectionEvolveCost() const {
        int cost = 0;
        for (const auto& u : units_) if (u.selected) cost += evolveCost(u);
        return cost;
    }
    float incomeRate() const {
        float r = state_ == State::Wave ? .6f : 0;
        for (const auto& u : units_) if (u.type == Digester) r += digestRate(u);
        return r;
    }

    void press(int button) {
        if (state_ == State::Title) { begin(); return; }
        if (button >= 0 && button < 4) {
            const int cost = ut(button).cost;
            if (biomass_ < cost) { toast("NOT ENOUGH BIOMASS"); return; }
            if (units_.size() >= 60) { toast("THE COLONY CANNOT FEED MORE MOUTHS"); return; }
            biomass_ -= cost;
            for (auto& u : units_) u.selected = false;
            const float a = frand() * 6.283f;
            addUnit(button, base_[0] - 2.6f + std::cos(a) * .4f, base_[1] + std::sin(a) * 1.4f, 1);
            units_.back().selected = true;
            sound(SBuy, 2, .05f);
            toast(std::string(ut(button).name) + " GROWN. RIGHT CLICK TO PLACE IT.");
        } else if (button == 4) {
            bool any = false;
            for (auto& u : units_) {
                if (!u.selected || u.level >= 5) continue;
                const int cost = evolveCost(u);
                if (biomass_ < cost) continue;
                biomass_ -= cost; ++u.level; any = true;
                const float old = u.maxhp;
                u.maxhp = ut(u.type).hp * (1 + .3f * (u.level - 1));
                u.hp += u.maxhp - old;
                rings_.push_back({u.x, u.y, .3f, 3.f, .35f});
            }
            if (any) sound(SEvolve, 5, .05f);
            else toast(selectionEvolveCost() ? "NOT ENOUGH BIOMASS" : "SELECT MOUTHS TO EVOLVE");
        } else if (button == 5) {
            const int cost = mouthCost();
            if (!cost) { toast("THE MOUTH IS FULLY GROWN"); return; }
            if (biomass_ < cost) { toast("NOT ENOUGH BIOMASS"); return; }
            biomass_ -= cost; ++mouthLevel_; baseMax_ += 20; baseHp_ += 20;
            rings_.push_back({base_[0], base_[1], 1.f, 6.f, .5f});
            sound(SEvolve, 5, .05f); toast("THE MOUTH GROWS ANOTHER ROW OF TEETH");
        } else if (button == 6) {
            const int cost = colonyCost();
            if (biomass_ < cost) { toast("NOT ENOUGH BIOMASS"); return; }
            biomass_ -= cost; ++colony_;
            sound(SEvolve, 5, .05f); toast("COLONY " + std::to_string(colony_) + ". ALL TEETH SHARPER. KILLS PAY MORE.");
        } else if (button == 7) {
            if (pulseCd_ > 0) { toast("THE PULSE IS STILL GATHERING"); return; }
            pulseCd_ = 40;
            for (int i = 0; i < int(blobs_.size()); ++i) {
                Blob& b = blobs_[i];
                const float dx = b.x - base_[0], dy = b.y - base_[1], d = std::sqrt(dx * dx + dy * dy);
                if (d < 9.f) {
                    damageBlob(i, 150.f * colonyMul());
                    b.vx += dx / (d + .1f) * 6; b.vy += dy / (d + .1f) * 6;
                }
            }
            rings_.push_back({base_[0], base_[1], 1.f, 22.f, .4f});
            shake_ = 1; sound(SPulse, 6, .1f);
        } else if (button == 8) {
            if (baseHp_ >= baseMax_) { toast("THE MOUTH IS WHOLE"); return; }
            if (biomass_ < 60) { toast("NOT ENOUGH BIOMASS"); return; }
            biomass_ -= 60; baseHp_ = std::min(baseMax_, baseHp_ + 30);
            sound(SBuy, 2, .05f);
        } else if (button == 9) {
            if (state_ != State::Build) return;
            const int bonus = int(std::max(0.f, intermission_) * (1.5f + wave_ * .25f));
            biomass_ += bonus;
            if (bonus > 0) toast("CALLED EARLY +" + std::to_string(bonus));
            startWave();
        }
    }

    void addUnit(int type, float x, float y, int level) {
        Unit u{};
        u.x = x; u.y = y; u.type = type; u.level = level; u.target = -1;
        u.maxhp = u.hp = ut(type).hp * (1 + .3f * (level - 1));
        u.tx = x; u.ty = y; u.face = 3.14159f; u.anim = frand() * 6.28f;
        pushOutOfWalls(u.x, u.y, ut(type).radius);
        units_.push_back(std::move(u));
    }

    void orderMove(float wx, float wy) {
        std::vector<int> chosen;
        for (int i = 0; i < int(units_.size()); ++i) if (units_[i].selected) chosen.push_back(i);
        if (chosen.empty()) return;
        mark_ = .6f; markX_ = wx; markY_ = wy;
        // Spread a group over a small spiral of open tiles around the click.
        int slot = 0;
        for (int i : chosen) {
            float tx = wx, ty = wy;
            for (; slot < 400; ++slot) {
                const float a = slot * 2.39996f, r = slot ? .55f * std::sqrt(float(slot)) : 0.f;
                tx = wx + std::cos(a) * r; ty = wy + std::sin(a) * r;
                const int X = int(tx), Y = int(ty);
                if (!solid(X, Y) && blobDist_[Y * MW + X] < kInf) break;
            }
            ++slot;
            Unit& u = units_[i];
            u.tx = tx; u.ty = ty; u.moving = true;
            computeField(u.path, tx, ty, .6f);
        }
    }

    // ---------------------------------------------------------------- waves
    void startWave() {
        if (state_ != State::Build) return;
        ++wave_;
        state_ = State::Wave;
        spawnQueue_.clear();
        float budget = 30.f * std::pow(1.27f, float(wave_ - 1));
        const int spawnsOpen = wave_ >= 6 ? 5 : 3;
        const float weight[5] = {1.f, wave_ >= 2 ? .9f : 0.f, wave_ >= 3 ? .5f : 0.f, wave_ >= 5 ? .35f : 0.f, wave_ >= 7 ? .25f : 0.f};
        while (budget > 0) {
            float total = 0; for (float wgt : weight) total += wgt;
            float pick = frand() * total; int kind = 0;
            for (; kind < 4; ++kind) { if (pick < weight[kind]) break; pick -= weight[kind]; }
            const int big = kind >= Splitter;
            const int clump = big ? 1 + int(frand() * 4) : 6 + int(frand() * 26);
            const int hole = int(frand() * spawnsOpen) % spawnsOpen;
            for (int i = 0; i < clump && budget > 0; ++i) {
                spawnQueue_.push_back({std::uint8_t(kind), std::uint8_t(hole)});
                budget -= sp(kind).cost;
            }
        }
        if (wave_ % 5 == 0) {
            for (int i = 0; i < wave_ / 5; ++i) {
                const size_t at = spawnQueue_.size() / 3 + size_t(frand() * spawnQueue_.size() / 2);
                spawnQueue_.insert(spawnQueue_.begin() + std::min(at, spawnQueue_.size()),
                                   {std::uint8_t(Brute), std::uint8_t(int(frand() * 3))});
            }
        }
        spawnRate_ = spawnQueue_.size() / (16.f + wave_ * 1.4f);
        spawnCarry_ = 0;
        sound(SWave, 3, .5f);
        toast("WAVE " + std::to_string(wave_) + "  " + std::to_string(spawnQueue_.size()) + " BLOBS");
    }

    void spawnStep(float dt) {
        spawnCarry_ += dt * spawnRate_;
        size_t taken = 0;
        while (spawnCarry_ >= 1 && taken < spawnQueue_.size() && blobs_.size() < size_t(kMaxBlobs)) {
            const auto [kind, hole] = spawnQueue_[taken++];
            const auto& s = spawns_[hole];
            spawnBlob(kind, s[0] + frand() * 1.6f - .8f, s[1] + frand() * 1.6f - .8f);
            spawnCarry_ -= 1;
        }
        spawnQueue_.erase(spawnQueue_.begin(), spawnQueue_.begin() + long(taken));
        if (blobs_.size() >= size_t(kMaxBlobs)) spawnCarry_ = std::min(spawnCarry_, 1.f);
    }

    void spawnBlob(int kind, float x, float y) {
        Blob b{};
        const auto& s = sp(kind);
        const float scale = 1.f + .09f * std::max(0, wave_ - 1);
        b.x = x; b.y = y; b.r = s.r * (.9f + frand() * .2f);
        b.hp = b.maxhp = s.hp * scale;
        b.phase = frand() * 6.283f; b.kind = std::uint8_t(kind); b.id = nextId_++;
        pushOutOfWalls(b.x, b.y, b.r);
        blobs_.push_back(b);
    }

    void waveCleared() {
        const int bonus = 20 + wave_ * 8;
        const int interest = std::min(120, int(biomass_ * .05f));
        biomass_ += bonus + interest;
        if (wave_ >= kWaves && !endless_) { state_ = State::Won; toast("THE TIDE BREAKS"); return; }
        state_ = State::Build; intermission_ = 14;
        toast("WAVE " + std::to_string(wave_) + " EATEN  +" + std::to_string(bonus) + "  INTEREST +" + std::to_string(interest));
    }

    // ---------------------------------------------------------------- blobs
    void buildHash() {
        cellHead_.assign(MW * MH, -1);
        cellNext_.assign(blobs_.size(), -1);
        for (int i = 0; i < int(blobs_.size()); ++i) {
            const Blob& b = blobs_[i];
            const int c = std::clamp(int(b.y), 0, MH - 1) * MW + std::clamp(int(b.x), 0, MW - 1);
            cellNext_[i] = cellHead_[c]; cellHead_[c] = i;
        }
    }

    template <class F> void forNear(float x, float y, float range, F&& fn) const {
        const int x0 = std::max(0, int(x - range)), x1 = std::min(MW - 1, int(x + range));
        const int y0 = std::max(0, int(y - range)), y1 = std::min(MH - 1, int(y + range));
        for (int cy = y0; cy <= y1; ++cy) for (int cx = x0; cx <= x1; ++cx)
            for (int j = cellHead_[cy * MW + cx]; j >= 0; j = cellNext_[j]) if (!fn(j)) return;
    }

    void stepBlobs(float dt) {
        const int n = int(blobs_.size());
        pushX_.assign(n, 0.f); pushY_.assign(n, 0.f);
        // Jellies make the crowd around them hurry and knit.
        for (int i = 0; i < n; ++i) {
            if (blobs_[i].kind != Jelly || blobs_[i].dead) continue;
            const float jx = blobs_[i].x, jy = blobs_[i].y;
            forNear(jx, jy, 2.f, [&](int j) {
                Blob& o = blobs_[j];
                const float dx = o.x - jx, dy = o.y - jy;
                if (dx * dx + dy * dy < 4.f) o.buff = .3f;
                return true;
            });
        }
        for (int i = 0; i < n; ++i) {
            Blob& a = blobs_[i];
            if (a.dead) continue;
            int checks = 0;
            const int cx = int(a.x), cy = int(a.y);
            for (int oy = -1; oy <= 1 && checks < 22; ++oy) for (int ox = -1; ox <= 1 && checks < 22; ++ox) {
                const int X = cx + ox, Y = cy + oy;
                if (X < 0 || Y < 0 || X >= MW || Y >= MH) continue;
                for (int j = cellHead_[Y * MW + X]; j >= 0 && checks < 22; j = cellNext_[j]) {
                    if (j == i) continue;
                    Blob& b = blobs_[j];
                    if (b.dead) continue;
                    ++checks;
                    float dx = a.x - b.x, dy = a.y - b.y;
                    float d2 = dx * dx + dy * dy;
                    const bool puddles = a.kind == Puddle && b.kind == Puddle;
                    const float reach = (a.r + b.r) * (puddles ? .8f : 1.05f);
                    if (d2 >= reach * reach) continue;
                    if (d2 < 1e-8f) { dx = std::cos(a.phase); dy = std::sin(a.phase); d2 = 1e-4f; }
                    const float d = std::sqrt(d2);
                    if (puddles && d < (a.r + b.r) * .55f && a.id < b.id && a.hp + b.hp < 260) {
                        // Two puddles that touch run together into one.
                        a.r = std::min(.8f, std::sqrt(a.r * a.r + b.r * b.r));
                        a.hp += b.hp; a.maxhp += b.maxhp;
                        b.dead = true; b.rewarded = false;
                        continue;
                    }
                    const float ma = sp(a.kind).mass * a.r, mb = sp(b.kind).mass * b.r;
                    const float share = mb / (ma + mb), overlap = (reach - d) / reach;
                    pushX_[i] += dx / d * overlap * share; pushY_[i] += dy / d * overlap * share;
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            Blob& a = blobs_[i];
            if (a.dead) continue;
            const Species& s = sp(a.kind);
            float dx, dy; sampleDir(a.x, a.y, dx, dy);
            float mul = 1;
            switch (a.kind) {
                case Mite: {
                    mul = .45f + 1.3f * std::max(0.f, std::sin(time_ * 7 + a.phase));
                    const float t = .55f * std::sin(time_ * 2.3f + a.phase * 3);
                    const float c = std::cos(t), sn = std::sin(t);
                    const float rx = dx * c - dy * sn, ry = dx * sn + dy * c; dx = rx; dy = ry;
                    break;
                }
                case Puddle: mul = 1.f / (1.f + (a.r - .24f) * 2.4f); break;
                case Jelly: mul = .7f + .55f * std::max(0.f, std::sin(time_ * 3 + a.phase)); break;
                case Brute: mul = .8f + .25f * std::sin(time_ * 1.3f + a.phase); break;
                default: break;
            }
            if (a.buff > 0) { mul *= 1.35f; a.hp = std::min(a.maxhp, a.hp + 2.f * dt); a.buff -= dt; }
            const float k = std::min(1.f, s.accel * dt);
            a.vx += (dx * s.speed * mul - a.vx) * k;
            a.vy += (dy * s.speed * mul - a.vy) * k;
            a.x += a.vx * dt + pushX_[i] * std::min(1.f, 14 * dt) * (a.r + .1f);
            a.y += a.vy * dt + pushY_[i] * std::min(1.f, 14 * dt) * (a.r + .1f);
            pushOutOfWalls(a.x, a.y, a.r * .9f);
            a.flash = std::max(0.f, a.flash - dt);
            const float bx = a.x - base_[0], by = a.y - base_[1];
            if (bx * bx + by * by < (1.8f + a.r) * (1.8f + a.r)) {
                baseHp_ -= s.bite; hurt_ = .25f; shake_ = std::min(1.f, shake_ + .15f + s.bite * .03f);
                ++lost_; killBlob(i, false); sound(SHurt, 4, .18f);
            }
        }
    }

    void damageBlob(int i, float amount) {
        Blob& b = blobs_[i];
        if (b.dead) return;
        b.hp -= amount * sp(b.kind).armor;
        b.flash = .07f;
        if (b.hp <= 0) killBlob(i, true);
    }

    void killBlob(int i, bool reward) {
        Blob& b = blobs_[i];
        if (b.dead) return;
        b.dead = true; b.rewarded = reward;
        const int bits = 3 + int(b.r * 14);
        for (int k = 0; k < bits; ++k) particle(b.x, b.y, 2.f + b.r * 5);
        stainAt(b.x, b.y, b.r);
        if (reward) {
            ++kills_;
            biomass_ += sp(b.kind).reward * rewardMul();
            sound(SSplat, 1, .05f);
            if (b.kind == Splitter) for (int k = 0; k < 3; ++k) pendingMites_.push_back({b.x, b.y, Mite});
        }
    }

    void particle(float x, float y, float speed) {
        if (particles_.size() > 5000) return;
        const float a = frand() * 6.283f, v = speed * (.3f + frand());
        particles_.push_back({x, y, std::cos(a) * v, std::sin(a) * v, .35f + frand() * .45f});
    }

    void stainAt(float x, float y, float r) {
        const int sx = int(x * 4), sy = int(y * 4), rad = std::max(1, int(r * 5));
        for (int dy = -rad; dy <= rad; ++dy) for (int dx = -rad; dx <= rad; ++dx) {
            if (dx * dx + dy * dy > rad * rad) continue;
            const int X = sx + dx, Y = sy + dy;
            if (X < 0 || Y < 0 || X >= MW * 4 || Y >= MH * 4) continue;
            float& s = stain_[size_t(Y) * MW * 4 + X];
            s = std::min(1.f, s + .22f);
        }
    }

    void compact() {
        remap_.assign(blobs_.size(), -1);
        size_t out = 0;
        for (size_t i = 0; i < blobs_.size(); ++i) {
            if (blobs_[i].dead) continue;
            remap_[i] = int(out);
            if (out != i) blobs_[out] = blobs_[i];
            ++out;
        }
        blobs_.resize(out);
        for (auto& u : units_) if (u.target >= 0) u.target = u.target < int(remap_.size()) ? remap_[u.target] : -1;
        for (auto& t : mouthTargets_) if (t >= 0) t = t < int(remap_.size()) ? remap_[t] : -1;
        for (const auto& p : pendingMites_) {
            spawnBlob(p.kind, p.x + frand() * .5f - .25f, p.y + frand() * .5f - .25f);
            Blob& b = blobs_.back();
            const float a = frand() * 6.283f; b.vx = std::cos(a) * 3; b.vy = std::sin(a) * 3;
        }
        pendingMites_.clear();
    }

    // ---------------------------------------------------------------- defenders
    int findTarget(float x, float y, float range, int skipA = -1, int skipB = -1) const {
        int best = -1; float bd = range * range;
        forNear(x, y, range, [&](int j) {
            const Blob& b = blobs_[j];
            if (b.dead || j == skipA || j == skipB) return true;
            const float dx = b.x - x, dy = b.y - y, d = dx * dx + dy * dy;
            if (d < bd && lineOfSight(x, y, b.x, b.y)) { bd = d; best = j; }
            return true;
        });
        return best;
    }

    bool validTarget(int t, float x, float y, float range) const {
        if (t < 0 || t >= int(blobs_.size()) || blobs_[t].dead) return false;
        const float dx = blobs_[t].x - x, dy = blobs_[t].y - y;
        return dx * dx + dy * dy <= range * range;
    }

    void stepUnits(float dt) {
        for (auto& u : units_) {
            const UnitType& T = ut(u.type);
            u.anim += dt;
            u.hurt = std::max(0.f, u.hurt - dt);
            if (u.moving) {
                float gx = u.tx - u.x, gy = u.ty - u.y;
                const float dist = std::sqrt(gx * gx + gy * gy);
                if (dist < .08f) { u.moving = false; }
                else {
                    const int X = int(u.x), Y = int(u.y);
                    const int here = Y * MW + X;
                    if (!u.path.empty() && u.path[here] > 1.f && u.path[here] < kInf) {
                        float bestD = u.path[here]; int bk = -1;
                        for (int k = 0; k < 8; ++k) {
                            const int NX = X + kNx[k], NY = Y + kNy[k];
                            if (solid(NX, NY) || (k >= 4 && (solid(NX, Y) || solid(X, NY)))) continue;
                            if (u.path[NY * MW + NX] < bestD) { bestD = u.path[NY * MW + NX]; bk = k; }
                        }
                        if (bk >= 0) { gx = X + kNx[bk] + .5f - u.x; gy = Y + kNy[bk] + .5f - u.y; }
                    }
                    const float n = std::sqrt(gx * gx + gy * gy) + 1e-6f;
                    const float step = std::min(dist, T.speed * dt);
                    u.x += gx / n * step; u.y += gy / n * step;
                    if (u.target < 0) u.face = std::atan2(gy, gx);
                    pushOutOfWalls(u.x, u.y, T.radius * .8f);
                }
            }
            // Blobs that reach a mouth chew on it and are shouldered aside.
            forNear(u.x, u.y, 1.5f, [&](int j) {
                Blob& b = blobs_[j];
                if (b.dead) return true;
                const float dx = b.x - u.x, dy = b.y - u.y, reach = T.radius + b.r;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= reach * reach) return true;
                const float d = std::sqrt(d2) + 1e-4f;
                b.x = u.x + dx / d * reach; b.y = u.y + dy / d * reach;
                b.vx *= .4f; b.vy *= .4f;
                u.hp -= sp(b.kind).touch * dt; u.hurt = 1.5f;
                return true;
            });
            if (u.hurt <= 0) u.hp = std::min(u.maxhp, u.hp + 4 * dt);
            if (u.type == Digester) continue;
            const float range = unitRange(u);
            u.cooldown = std::max(0.f, u.cooldown - dt);
            u.retarget -= dt;
            if (!validTarget(u.target, u.x, u.y, range) || u.retarget <= 0) {
                u.target = findTarget(u.x, u.y, range);
                u.retarget = .3f;
            }
            if (u.target < 0) continue;
            Blob& t = blobs_[u.target];
            u.face = std::atan2(t.y - u.y, t.x - u.x);
            if (u.type == Biter) {
                const float tx = t.x, ty = t.y;
                damageBlob(u.target, unitDamage(u) * dt);
                beams_.push_back({u.x, u.y, tx, ty, dt * 1.6f, 1.f + (u.level >= 3)});
                sound(SLaser, 0, .09f);
            } else if (u.type == Stalk && u.cooldown <= 0) {
                u.cooldown = T.interval;
                const float dx = t.x - u.x, dy = t.y - u.y, len = std::sqrt(dx * dx + dy * dy) + 1e-4f;
                const float ex = u.x + dx / len * range, ey = u.y + dy / len * range;
                std::vector<std::pair<float, int>> hits;
                const float cxm = (u.x + ex) / 2, cym = (u.y + ey) / 2;
                forNear(cxm, cym, range / 2 + 1, [&](int j) {
                    const Blob& b = blobs_[j];
                    if (b.dead) return true;
                    const float px = b.x - u.x, py = b.y - u.y;
                    const float along = (px * dx + py * dy) / len;
                    if (along < 0 || along > range) return true;
                    const float off = std::abs(px * dy - py * dx) / len;
                    if (off < b.r + .15f) hits.push_back({along, j});
                    return true;
                });
                std::sort(hits.begin(), hits.end());
                float reachEnd = range;
                int count = 0;
                for (const auto& h : hits) {
                    const Blob& b = blobs_[h.second];
                    if (!lineOfSight(u.x, u.y, b.x, b.y)) { reachEnd = h.first; break; }
                    damageBlob(h.second, unitDamage(u));
                    if (++count >= 6) { reachEnd = h.first; break; }
                }
                beams_.push_back({u.x, u.y, u.x + dx / len * reachEnd, u.y + dy / len * reachEnd, .16f, 2.f + (u.level >= 3)});
                sound(SLaser, 0, .06f);
            } else if (u.type == Maw && u.cooldown <= 0) {
                u.cooldown = T.interval;
                const float dmg = unitDamage(u), ux = u.x, uy = u.y;
                forNear(ux, uy, range, [&](int j) {
                    const float dx = blobs_[j].x - ux, dy = blobs_[j].y - uy;
                    if (dx * dx + dy * dy <= range * range) damageBlob(j, dmg);
                    return true;
                });
                rings_.push_back({ux, uy, .3f, range * 3.2f, .28f});
                sound(SSplat, 1, .05f);
            }
        }
        for (size_t i = 0; i < units_.size();) {
            if (units_[i].hp > 0) { ++i; continue; }
            for (int k = 0; k < 30; ++k) particle(units_[i].x, units_[i].y, 5.f);
            stainAt(units_[i].x, units_[i].y, .8f);
            sound(SHurt, 4, .1f);
            toast(std::string(ut(units_[i].type).name) + " WAS EATEN");
            units_.erase(units_.begin() + long(i));
        }
    }

    void stepMouth(float dt) {
        const float range = mouthRange();
        const int beams = mouthBeams();
        for (int k = 0; k < 3; ++k) {
            int& t = mouthTargets_[k];
            if (k >= beams) { t = -1; continue; }
            if (!validTarget(t, base_[0], base_[1], range))
                t = findTarget(base_[0], base_[1], range, k > 0 ? mouthTargets_[0] : -1, k > 1 ? mouthTargets_[1] : -1);
            if (t < 0) continue;
            const float tx = blobs_[t].x, ty = blobs_[t].y;
            damageBlob(t, mouthDps() * dt);
            beams_.push_back({base_[0], base_[1], tx, ty, dt * 1.6f, 3.f});
            sound(SLaser, 0, .1f);
        }
    }

    void stepParticles(float dt) {
        for (size_t i = 0; i < particles_.size();) {
            Particle& p = particles_[i];
            p.x += p.vx * dt; p.y += p.vy * dt;
            const float drag = std::max(0.f, 1 - 4 * dt);
            p.vx *= drag; p.vy *= drag; p.life -= dt;
            if (p.life <= 0) { p = particles_.back(); particles_.pop_back(); } else ++i;
        }
        for (size_t i = 0; i < beams_.size();) {
            beams_[i].life -= dt;
            if (beams_[i].life <= 0) { beams_[i] = beams_.back(); beams_.pop_back(); } else ++i;
        }
        for (size_t i = 0; i < rings_.size();) {
            rings_[i].life -= dt; rings_[i].r += rings_[i].grow * dt;
            if (rings_[i].life <= 0) { rings_[i] = rings_.back(); rings_.pop_back(); } else ++i;
        }
        mark_ = std::max(0.f, mark_ - dt);
    }

    // ---------------------------------------------------------------- canvas
    struct Canvas {
        std::uint32_t* p; int w, h;
        void set(int x, int y, bool on = true) {
            if (unsigned(x) < unsigned(w) && unsigned(y) < unsigned(h)) p[size_t(y) * w + x] = on ? 0xffffffu : 0u;
        }
        void fill(int x0, int y0, int x1, int y1, bool on = true) {
            x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(w, x1); y1 = std::min(h, y1);
            for (int y = y0; y < y1; ++y) std::fill(p + size_t(y) * w + x0, p + size_t(y) * w + std::max(x0, x1), on ? 0xffffffu : 0u);
        }
        void frame(int x0, int y0, int x1, int y1, bool on = true) {
            for (int x = x0; x < x1; ++x) { set(x, y0, on); set(x, y1 - 1, on); }
            for (int y = y0; y < y1; ++y) { set(x0, y, on); set(x1 - 1, y, on); }
        }
        void dashed(int x0, int y0, int x1, int y1, int dash) {
            for (int x = x0; x <= x1; ++x) if ((x / dash) & 1) { set(x, y0); set(x, y1); }
            for (int y = y0; y <= y1; ++y) if ((y / dash) & 1) { set(x0, y); set(x1, y); }
        }
        void dot(float x, float y, int size, bool on = true) {
            const int ix = int(x) - size / 2, iy = int(y) - size / 2;
            for (int j = 0; j < size; ++j) for (int i = 0; i < size; ++i) set(ix + i, iy + j, on);
        }
        void line(float x0, float y0, float x1, float y1, float thick, bool on = true) {
            const float dx = x1 - x0, dy = y1 - y0;
            const int n = std::max(1, int(std::max(std::abs(dx), std::abs(dy))));
            const int size = std::max(1, int(thick + .5f));
            for (int i = 0; i <= n; ++i) dot(x0 + dx * i / n, y0 + dy * i / n, size, on);
        }
        void disc(float cx, float cy, float r, bool on = true) {
            const int y0 = int(cy - r - 1), y1 = int(cy + r + 1), x0 = int(cx - r - 1), x1 = int(cx + r + 1);
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                const float dx = x + .5f - cx, dy = y + .5f - cy;
                if (dx * dx + dy * dy <= r * r) set(x, y, on);
            }
        }
        void ring(float cx, float cy, float r, float t, bool on = true) {
            const int y0 = int(cy - r - t - 1), y1 = int(cy + r + t + 1), x0 = int(cx - r - t - 1), x1 = int(cx + r + t + 1);
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                const float dx = x + .5f - cx, dy = y + .5f - cy, d = std::sqrt(dx * dx + dy * dy);
                if (std::abs(d - r) <= t * .5f) set(x, y, on);
            }
        }
        void triangle(float ax, float ay, float bx, float by, float cx, float cy, bool on = true) {
            const int x0 = int(std::min({ax, bx, cx})), x1 = int(std::max({ax, bx, cx})) + 1;
            const int y0 = int(std::min({ay, by, cy})), y1 = int(std::max({ay, by, cy})) + 1;
            auto edge = [](float x0, float y0, float x1, float y1, float x, float y) { return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0); };
            const float area = edge(ax, ay, bx, by, cx, cy);
            if (std::abs(area) < 1e-3f) return;
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                const float px = x + .5f, py = y + .5f;
                const float e0 = edge(ax, ay, bx, by, px, py), e1 = edge(bx, by, cx, cy, px, py), e2 = edge(cx, cy, ax, ay, px, py);
                if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) set(x, y, on);
            }
        }
    };
    static int bayer(int x, int y) {
        static const int b[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        return b[y & 3][x & 3];
    }

    // A 5x7 pixel face keeps every label as sharp as the rest of Anaise.
    static const char* const* glyph(char ch) {
        struct G { char c; const char* rows[7]; };
        static const G font[] = {
            {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
            {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
            {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
            {'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
            {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
            {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
            {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
            {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
            {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
            {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
            {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
            {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
            {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
            {'N', {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
            {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
            {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
            {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
            {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
            {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
            {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
            {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
            {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
            {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
            {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
            {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
            {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
            {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
            {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
            {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
            {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
            {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
            {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
            {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
            {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
            {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
            {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
            {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
            {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
            {'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
            {':', {".....", "..#..", "..#..", ".....", "..#..", "..#..", "....."}},
            {'.', {".....", ".....", ".....", ".....", ".....", "..#..", "..#.."}},
            {',', {".....", ".....", ".....", ".....", "..#..", "..#..", ".#..."}},
            {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
            {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
            {'%', {"##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##"}},
            {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
            {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
            {'[', {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."}},
            {']', {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."}},
            {'\'', {"..#..", "..#..", ".#...", ".....", ".....", ".....", "....."}},
            {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
            {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
            {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
        };
        if (ch >= 'a' && ch <= 'z') ch = char(ch - 32);
        for (const auto& g : font) if (g.c == ch) return g.rows;
        return nullptr;
    }
    static int textWidth(const std::string& s, int scale) { return s.empty() ? 0 : (int(s.size()) * 6 - 1) * scale; }
    static void text(Canvas& c, int x, int y, const std::string& s, int scale, bool on = true) {
        for (char ch : s) {
            if (const char* const* rows = glyph(ch)) {
                for (int r = 0; r < 7; ++r) for (int k = 0; k < 5; ++k)
                    if (rows[r][k] == '#') c.fill(x + k * scale, y + r * scale, x + (k + 1) * scale, y + (r + 1) * scale, on);
            }
            x += 6 * scale;
        }
    }
    static void textCentered(Canvas& c, int cx, int y, const std::string& s, int scale, bool on = true) {
        text(c, cx - textWidth(s, scale) / 2, y, s, scale, on);
    }

    // ---------------------------------------------------------------- layout
    struct Layout {
        float s, ox, oy; int ui, hudH, shopY, shopH, tipY;
        int bx[kButtons], bw[kButtons], by, bh;
    };
    Layout layout(int w, int h) const {
        Layout L{};
        L.ui = std::max(1, std::min(w / 640, h / 440));
        L.hudH = 14 * L.ui + 10;
        L.bh = 26 * L.ui + 6;
        L.shopH = L.bh + 8 * L.ui + 12;
        L.shopY = h - L.shopH;
        L.by = h - L.bh - 4;
        L.tipY = L.shopY + 3;
        const float mapH = float(L.shopY - L.hudH - 6), mapW = float(w - 8);
        L.s = std::max(2.f, std::min(mapW / MW, mapH / MH));
        L.ox = (w - MW * L.s) / 2;
        L.oy = L.hudH + 3 + (mapH - MH * L.s) / 2;
        if (shake_ > 0) { L.ox += std::sin(time_ * 91) * shake_ * 3; L.oy += std::cos(time_ * 77) * shake_ * 3; }
        const int gap = 3, total = w - 8 - gap * (kButtons - 1);
        int x = 4;
        for (int i = 0; i < kButtons; ++i) {
            L.bx[i] = x; L.bw[i] = total / kButtons + (i < total % kButtons);
            x += L.bw[i] + gap;
        }
        return L;
    }
    static int buttonAt(const Layout& L, int x, int y) {
        if (y < L.by || y >= L.by + L.bh) return -1;
        for (int i = 0; i < kButtons; ++i) if (x >= L.bx[i] && x < L.bx[i] + L.bw[i]) return i;
        return -1;
    }
    static bool toWorld(const Layout& L, int x, int y, float& wx, float& wy) {
        wx = (x - L.ox) / L.s; wy = (y - L.oy) / L.s;
        return wx >= 0 && wy >= 0 && wx < MW && wy < MH && y < L.shopY;
    }
    static void toWorldClamped(const Layout& L, int x, int y, float& wx, float& wy) {
        wx = std::clamp((x - L.ox) / L.s, 0.f, float(MW)); wy = std::clamp((y - L.oy) / L.s, 0.f, float(MH));
    }

    // ---------------------------------------------------------------- painting
    void drawMap(Canvas& c, const Layout& L) const {
        const int x0 = std::max(0, int(L.ox)), x1 = std::min(c.w, int(L.ox + MW * L.s));
        const int y0 = std::max(0, int(L.oy)), y1 = std::min(c.h, int(L.oy + MH * L.s));
        const float edge = 1.6f / L.s;
        std::vector<int> colTile(x1 > x0 ? x1 - x0 : 0), colStain(colTile.size());
        std::vector<float> colLocal(colTile.size());
        for (int x = x0; x < x1; ++x) {
            const float wx = (x + .5f - L.ox) / L.s;
            colTile[x - x0] = std::clamp(int(wx), 0, MW - 1);
            colLocal[x - x0] = wx - colTile[x - x0];
            colStain[x - x0] = std::clamp(int(wx * 4), 0, MW * 4 - 1);
        }
        for (int y = y0; y < y1; ++y) {
            const float wy = (y + .5f - L.oy) / L.s;
            const int ty = std::clamp(int(wy), 0, MH - 1);
            const float ly = wy - ty;
            const int sy = std::clamp(int(wy * 4), 0, MH * 4 - 1);
            std::uint32_t* row = c.p + size_t(y) * c.w;
            for (int x = x0; x < x1; ++x) {
                const int tx = colTile[x - x0];
                const float lx = colLocal[x - x0];
                bool on;
                if (wall_[ty * MW + tx]) {
                    const bool border = (lx < edge && !solid(tx - 1, ty)) || (lx > 1 - edge && !solid(tx + 1, ty)) ||
                                        (ly < edge && !solid(tx, ty - 1)) || (ly > 1 - edge && !solid(tx, ty + 1));
                    on = border || ((x + y) % 4 == 0);
                } else {
                    const float st = stain_[size_t(sy) * MW * 4 + colStain[x - x0]];
                    on = st > 0 && st * 7.f > bayer(x, y) + 1 && ((x * 7 + y * 3) % 5 != 0);
                    if (!on && ((tx + ty) & 1) == 0 && lx * L.s < 1 && ly * L.s < 1) on = true;
                }
                if (on) row[x] = 0xffffffu;
            }
        }
    }

    void drawSpawns(Canvas& c, const Layout& L) const {
        const int open = wave_ >= 6 || (state_ == State::Build && wave_ >= 5) ? 5 : 3;
        for (int i = 0; i < open; ++i) {
            const float x = L.ox + spawns_[i][0] * L.s, y = L.oy + spawns_[i][1] * L.s;
            const float speed = state_ == State::Wave ? 1.6f : .5f;
            for (int k = 0; k < 3; ++k) {
                const float f = std::fmod(time_ * speed + k / 3.f + i * .17f, 1.f);
                c.ring(x, y, (.35f + f * 1.1f) * L.s, 1.f);
            }
            c.disc(x, y, .5f * L.s, false);
            c.ring(x, y, .5f * L.s, 1.5f);
            for (int k = 0; k < 6; ++k) {
                const float a = k * 1.047f + time_ * .4f;
                c.line(x + std::cos(a) * .5f * L.s, y + std::sin(a) * .5f * L.s,
                       x + std::cos(a + .3f) * .8f * L.s, y + std::sin(a + .3f) * .8f * L.s, 1);
            }
        }
    }

    void drawBase(Canvas& c, const Layout& L) const {
        const float bx = L.ox + base_[0] * L.s, by = L.oy + base_[1] * L.s, R = 2.1f * L.s;
        // Tentacles writhe from the mouth; more of them with every evolution.
        const int arms = 6 + mouthLevel_ * 2;
        for (int a = 0; a < arms; ++a) {
            const float ang = a * 6.2832f / arms + .25f * std::sin(time_ * .8f + a);
            float px = bx + std::cos(ang) * R * .8f, py = by + std::sin(ang) * R * .8f;
            for (int k = 1; k <= 12; ++k) {
                const float f = k / 12.f, rr = R * (.8f + 1.3f * f);
                const float side = std::sin(k * .7f - time_ * 3 + a * 1.7f) * R * .22f * f;
                const float nx = bx + std::cos(ang) * rr - std::sin(ang) * side;
                const float ny = by + std::sin(ang) * rr + std::cos(ang) * side;
                c.line(px, py, nx, ny, std::max(1.f, 3.2f * (1 - f) * L.s / 10));
                px = nx; py = ny;
            }
        }
        c.disc(bx, by, R);
        if (hurt_ > 0) c.ring(bx, by, R + 3, 2, false);
        const bool firing = mouthTargets_[0] >= 0;
        const float open = std::clamp(.28f + .14f * std::sin(time_ * 2.4f) + (firing ? .35f : 0.f), .12f, .85f);
        const float hw = R * .74f, hh = R * .62f * open;
        for (int y = int(by - hh) - 1; y <= int(by + hh) + 1; ++y) for (int x = int(bx - hw) - 1; x <= int(bx + hw) + 1; ++x) {
            const float dx = (x + .5f - bx) / hw, dy = (y + .5f - by) / std::max(1.f, hh);
            if (dx * dx + dy * dy <= 1) c.set(x, y, false);
        }
        const int teeth = 5 + mouthLevel_;
        for (int i = 0; i < teeth; ++i) {
            const float f = (i + .5f) / teeth, x = bx - hw + f * 2 * hw, half = hw / teeth * .8f;
            const float lip = std::sqrt(std::max(0.f, 1 - (2 * f - 1) * (2 * f - 1))) * hh;
            const float len = std::max(1.5f, lip * .75f);
            c.triangle(x - half, by - lip, x + half, by - lip, x, by - lip + len);
            c.triangle(x - half, by + lip, x + half, by + lip, x, by + lip - len);
        }
    }

    static float fastAtan2(float y, float x) {
        const float ax = std::abs(x), ay = std::abs(y), mn = std::min(ax, ay), mx = std::max(ax, ay) + 1e-12f;
        const float a = mn / mx, s = a * a;
        float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
        if (ay > ax) r = 1.57079637f - r;
        if (x < 0) r = 3.14159274f - r;
        return y < 0 ? -r : r;
    }

    float shapeRadius(const Blob& b, float th, float heading) const {
        const float t = time_, ph = b.phase;
        switch (b.kind) {
            case Mite: { const float rel = th - heading; return .78f + .34f * std::cos(rel) + .08f * std::sin(3 * rel + ph); }
            case Puddle: return 1.f + .14f * std::sin(5 * th + t * 2 + ph) + .1f * std::sin(2 * th + ph);
            case Splitter: return 1.f + .2f * std::sin(4 * th + ph + t * .8f);
            case Shell: {
                const float a = std::fmod(th - ph + 62.8318f, 1.0472f) - .5236f;
                return .97f / std::cos(a) * .92f;
            }
            case Jelly: return 1.f + .17f * std::sin(8 * th + t * 4 + ph);
            default: return 1.f + .13f * std::sin(11 * th + ph) + .09f * std::sin(3 * th + t * 1.5f + ph);
        }
    }

    void drawBlob(Canvas& c, const Layout& L, const Blob& b) const {
        const float cx = L.ox + b.x * L.s, cy = L.oy + b.y * L.s, R = b.r * L.s;
        const float heading = std::atan2(b.vy, b.vx);
        if (R < 1.7f) { c.dot(cx, cy, b.kind == Brute ? 3 : 2); return; }
        if (b.kind == Jelly || b.kind == Brute) {
            // Trailing tentacles behind jellies; legs under brutes.
            const int legs = b.kind == Jelly ? 4 : 8;
            for (int k = 0; k < legs; ++k) {
                const float base = b.kind == Jelly ? heading + 3.1416f + (k - 1.5f) * .35f : k * .785f + b.phase;
                float px = cx, py = cy;
                for (int s = 1; s <= 6; ++s) {
                    const float f = s / 6.f, rr = R * (.8f + f * (b.kind == Jelly ? 1.4f : .7f));
                    const float sw = std::sin(time_ * 6 + s * .9f + k + b.phase) * R * .25f * f;
                    const float nx = cx + std::cos(base) * rr - std::sin(base) * sw;
                    const float ny = cy + std::sin(base) * rr + std::cos(base) * sw;
                    c.line(px, py, nx, ny, 1); px = nx; py = ny;
                }
            }
        }
        // The outline is sampled once per blob and looked up by angle.
        std::array<float, 64> outline;
        float most = 0;
        for (int k = 0; k < 64; ++k) { outline[k] = shapeRadius(b, k * 6.2831853f / 64 - 3.1415927f, heading); most = std::max(most, outline[k]); }
        const float lim = R * most + 1.f;
        const int x0 = int(cx - lim), x1 = int(cx + lim) + 1, y0 = int(cy - lim), y1 = int(cy + lim) + 1;
        const float ch = std::cos(heading), sh = std::sin(heading);
        // Inside the smallest radius the outline cannot reach, so the costly
        // angle lookup is only paid near the rim.
        static const float kCore[KindCount] = {.42f, .74f, .78f, .82f, .81f, .76f};
        const float core = R * kCore[b.kind] - 1.4f, core2 = core > 0 ? core * core : -1.f, lim2 = lim * lim;
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            float dx = x + .5f - cx, dy = y + .5f - cy;
            if (b.kind == Puddle) dy *= 1.35f;
            const float d2 = dx * dx + dy * dy;
            if (d2 > lim2) continue;
            const float d = std::sqrt(d2);
            float rr = R;
            bool rim = false;
            if (d2 >= core2) {
                const float a = (fastAtan2(dy, dx) + 3.1415927f) * (64 / 6.2831853f);
                const int k0 = int(a) & 63; const float f = a - std::floor(a);
                rr = R * (outline[k0] * (1 - f) + outline[(k0 + 1) & 63] * f);
                if (d > rr) continue;
                rim = d > rr - 1.3f;
            }
            bool on = true;
            if (!rim) {
                switch (b.kind) {
                    case Puddle: on = bayer(x, y) < 7; break;
                    case Splitter: on = std::abs(dx * -sh + dy * ch) > .9f && d > 1.2f; break;
                    case Shell: on = std::abs(d - rr * .55f) > .8f; break;
                    case Jelly: on = bayer(x, y) < 5 || d < R * .3f; break;
                    case Brute: {
                        on = std::abs(d - rr * .8f) > .9f;
                        for (int e = 0; e < 3; ++e) {
                            const float ea = heading + (e - 1) * .5f;
                            const float ex = std::cos(ea) * R * .45f - dx, ey = std::sin(ea) * R * .45f - dy;
                            if (ex * ex + ey * ey < R * R * .02f) on = false;
                        }
                        break;
                    }
                    default: break;
                }
                if (b.flash > 0) on = !on;
            }
            if (on) c.set(x, y);
        }
    }

    void drawUnitLegs(Canvas& c, const Layout& L, const Unit& u) const {
        const float cx = L.ox + u.x * L.s, cy = L.oy + u.y * L.s, R = ut(u.type).radius * L.s;
        const int arms = 2 + u.level;
        for (int a = 0; a < arms; ++a) {
            const float ang = a * 6.2832f / arms + u.anim * .4f;
            float px = cx, py = cy;
            for (int s = 1; s <= 6; ++s) {
                const float f = s / 6.f, rr = R * (.9f + f * 1.1f);
                const float sw = std::sin(u.anim * 4 + s * .8f + a * 2) * R * .3f * f;
                const float nx = cx + std::cos(ang) * rr - std::sin(ang) * sw;
                const float ny = cy + std::sin(ang) * rr + std::cos(ang) * sw;
                c.line(px, py, nx, ny, 1); px = nx; py = ny;
            }
        }
    }

    void drawUnit(Canvas& c, const Layout& L, const Unit& u) const {
        const float cx = L.ox + u.x * L.s, cy = L.oy + u.y * L.s, R = std::max(4.f, ut(u.type).radius * L.s * 1.25f);
        const float fx = std::cos(u.face), fy = std::sin(u.face);
        switch (u.type) {
            case Biter: {
                c.disc(cx, cy, R);
                const float gape = .45f + .25f * std::sin(u.anim * 9);
                c.triangle(cx, cy, cx + std::cos(u.face - gape) * R * 1.3f, cy + std::sin(u.face - gape) * R * 1.3f,
                           cx + std::cos(u.face + gape) * R * 1.3f, cy + std::sin(u.face + gape) * R * 1.3f, false);
                break;
            }
            case Stalk:
                c.disc(cx, cy, R); c.disc(cx, cy, R * .72f, false);
                c.disc(cx + fx * R * .3f, cy + fy * R * .3f, R * .38f);
                break;
            case Maw: {
                c.ring(cx, cy, R * .8f, std::max(2.f, R * .35f));
                const int teeth = 6 + u.level;
                for (int k = 0; k < teeth; ++k) {
                    const float a = k * 6.2832f / teeth + u.anim;
                    c.line(cx + std::cos(a) * R * .65f, cy + std::sin(a) * R * .65f, cx + std::cos(a) * R * .3f, cy + std::sin(a) * R * .3f, 1);
                }
                break;
            }
            default: {
                for (int y = int(cy - R); y <= int(cy + R); ++y) for (int x = int(cx - R); x <= int(cx + R); ++x) {
                    const float dx = x + .5f - cx, dy = y + .5f - cy;
                    if (dx * dx + dy * dy <= R * R && (dx * dx + dy * dy > (R - 1.3f) * (R - 1.3f) || bayer(x, y) < 6)) c.set(x, y);
                }
                for (int k = 0; k < 3; ++k) {
                    const float f = std::fmod(u.anim * .7f + k / 3.f, 1.f);
                    c.ring(cx + std::sin(k * 2.f + u.anim) * R * .3f, cy + R * (.5f - f * 1.2f), 1.2f + f * 1.5f, 1);
                }
                break;
            }
        }
        for (int k = 1; k < u.level; ++k) c.set(int(cx - R + k * 3), int(cy + R + 3));
        if (u.selected) {
            const int x0 = int(cx - R - 4), y0 = int(cy - R - 4), x1 = int(cx + R + 4), y1 = int(cy + R + 4), k = 3;
            for (int i = 0; i < k; ++i) {
                c.set(x0 + i, y0); c.set(x0, y0 + i); c.set(x1 - i, y0); c.set(x1, y0 + i);
                c.set(x0 + i, y1); c.set(x0, y1 - i); c.set(x1 - i, y1); c.set(x1, y1 - i);
            }
            if (u.moving) {
                const float tx = L.ox + u.tx * L.s, ty = L.oy + u.ty * L.s;
                c.line(tx - 2, ty - 2, tx + 2, ty + 2, 1); c.line(tx - 2, ty + 2, tx + 2, ty - 2, 1);
            }
        }
        if (u.hp < u.maxhp) {
            const int w = int(R * 2), x = int(cx - R), y = int(cy - R - 5);
            c.frame(x - 1, y - 1, x + w + 1, y + 2);
            c.fill(x, y, x + int(w * std::max(0.f, u.hp / u.maxhp)), y + 1);
        }
    }

    void drawBeam(Canvas& c, const Layout& L, const Beam& b) const {
        const float x0 = L.ox + b.x0 * L.s, y0 = L.oy + b.y0 * L.s, x1 = L.ox + b.x1 * L.s, y1 = L.oy + b.y1 * L.s;
        const float wobble = std::sin(time_ * 80 + x1) * .6f;
        c.line(x0, y0, x1 + wobble, y1 - wobble, b.width);
        c.disc(x1, y1, 1.5f + b.width * .8f);
    }

    void drawHud(Canvas& c, const Layout& L) const {
        c.fill(0, 0, c.w, L.hudH, false);
        c.fill(0, L.hudH - 1, c.w, L.hudH);
        const std::string waveText = "WAVE " + std::to_string(wave_) + (endless_ ? "" : "/" + std::to_string(kWaves));
        char income[16]; std::snprintf(income, sizeof income, "%.1f", incomeRate());
        const std::string parts[] = {
            waveText,
            "BIOMASS " + std::to_string(int(biomass_)) + " +" + income + "/S",
            "MOUTH " + std::to_string(int(std::ceil(baseHp_))) + "/" + std::to_string(int(baseMax_)),
            "BLOBS " + std::to_string(blobs_.size() + spawnQueue_.size()),
            "KILLS " + std::to_string(kills_),
        };
        int total = 0; for (const auto& p : parts) total += textWidth(p, 1);
        int scale = L.ui * 2;
        while (scale > 1 && total * scale + 16 * scale * 4 > c.w) --scale;
        const int gap = std::max(8, (c.w - 12 - total * scale) / 4);
        int x = 6;
        const int y = (L.hudH - 7 * scale) / 2;
        for (int i = 0; i < 5; ++i) {
            text(c, x, y, parts[i], scale);
            if (i == 2 && baseHp_ < baseMax_ * .3f && std::fmod(time_, .6f) < .3f) c.frame(x - 3, y - 2, x + textWidth(parts[i], scale) + 3, y + 7 * scale + 2);
            x += textWidth(parts[i], scale) + gap;
        }
    }

    std::string tip() const {
        if (toastTime_ > 0) return toast_;
        if (hover_ >= 0 && hover_ < 4) return std::string(ut(hover_).blurb) + "  COST " + std::to_string(ut(hover_).cost);
        switch (hover_) {
            case 4: return "EVOLVE SELECTED MOUTHS: MORE TEETH, RANGE AND FLESH. MAX 5.";
            case 5: return "GROW THE GREAT MOUTH: HOTTER LASER, MORE RANGE, EXTRA BEAMS AT 3 AND 5.";
            case 6: return "COLONY: +12% DAMAGE AND +10% BIOMASS PER KILL FOR EVERYTHING.";
            case 7: return "PULSE: THE MOUTH SCREAMS. EVERYTHING NEAR IT BURSTS. 40S TO REGROW.";
            case 8: return "REPAIR: 60 BIOMASS HEALS THE MOUTH BY 30.";
            case 9: return "CALL THE NEXT WAVE NOW. TIME LEFT PAYS AS BIOMASS.";
            default: break;
        }
        int selected = 0; for (const auto& u : units_) selected += u.selected;
        if (selected) return std::to_string(selected) + " SELECTED. RIGHT CLICK TO MOVE. E TO EVOLVE.";
        if (state_ == State::Build) return "NEXT WAVE IN " + std::to_string(int(std::ceil(std::max(0.f, intermission_)))) + ". SPACE CALLS IT EARLY.";
        return "DRAG TO SELECT. RIGHT CLICK TO MOVE. 1-4 GROWS NEW MOUTHS.";
    }

    void drawShop(Canvas& c, const Layout& L) const {
        c.fill(0, L.shopY, c.w, c.h, false);
        c.fill(0, L.shopY, c.w, L.shopY + 1);
        std::string t = tip();
        while (textWidth(t, L.ui) > c.w - 12 && t.size() > 4) t.pop_back();
        text(c, 6, L.tipY + 2 * L.ui, t, L.ui);
        const int evolve = selectionEvolveCost();
        for (int i = 0; i < kButtons; ++i) {
            std::string top, bottom; bool enabled = true;
            if (i < 4) { top = std::to_string(i + 1) + " " + ut(i).name; bottom = std::to_string(ut(i).cost); enabled = biomass_ >= ut(i).cost; }
            else if (i == 4) { top = "E EVOLVE"; bottom = evolve ? std::to_string(evolve) : "-"; enabled = evolve > 0 && biomass_ >= evolve; }
            else if (i == 5) { top = "U MOUTH"; bottom = mouthCost() ? std::to_string(mouthCost()) : "MAX"; enabled = mouthCost() && biomass_ >= mouthCost(); }
            else if (i == 6) { top = "C COLONY"; bottom = std::to_string(colonyCost()); enabled = biomass_ >= colonyCost(); }
            else if (i == 7) { top = "Q PULSE"; bottom = pulseCd_ > 0 ? std::to_string(int(std::ceil(pulseCd_))) + "S" : "READY"; enabled = pulseCd_ <= 0; }
            else if (i == 8) { top = "H REPAIR"; bottom = "60"; enabled = biomass_ >= 60 && baseHp_ < baseMax_; }
            else { top = "SPACE"; bottom = state_ == State::Build ? "WAVE " + std::to_string(wave_ + 1) : "..."; enabled = state_ == State::Build; }
            const int x0 = L.bx[i], x1 = x0 + L.bw[i], y0 = L.by, y1 = L.by + L.bh;
            const bool hot = hover_ == i && enabled;
            if (hot) c.fill(x0, y0, x1, y1);
            if (enabled) c.frame(x0, y0, x1, y1, true);
            else for (int x = x0; x < x1; ++x) for (int y = y0; y < y1; ++y)
                if ((x == x0 || x == x1 - 1 || y == y0 || y == y1 - 1) && ((x + y) & 1)) c.set(x, y);
            int scale = L.ui;
            while (scale > 1 && textWidth(top, scale) > L.bw[i] - 6) --scale;
            std::string a = top, b = bottom;
            while (textWidth(a, scale) > L.bw[i] - 4 && a.size() > 1) a.pop_back();
            const int cx = (x0 + x1) / 2;
            textCentered(c, cx, y0 + 4 * L.ui, a, scale, !hot);
            textCentered(c, cx, y0 + 15 * L.ui, b, scale, !hot);
            if (!enabled) {
                // Unaffordable costs are shown at half strength.
                for (int y = y0 + 15 * L.ui; y < y0 + 22 * L.ui; ++y)
                    for (int x = x0 + 1; x < x1 - 1; ++x) if ((x + y) & 1) c.set(x, y, false);
            }
        }
    }

    void panel(Canvas& c, int cx, int cy, int w, int h) const {
        c.fill(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, false);
        c.frame(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
        c.frame(cx - w / 2 + 3, cy - h / 2 + 3, cx + w / 2 - 3, cy + h / 2 - 3);
    }

    void drawOverlay(Canvas& c, const Layout& L, double clockSec) const {
        const int cx = c.w / 2, cy = (L.hudH + L.shopY) / 2, u = L.ui;
        if (state_ == State::Title) {
            const int big = std::max(3, std::min(8, c.w / 90));
            panel(c, cx, cy, std::min(c.w - 20, textWidth("TENTACLE", big) + 60 * u + 140), 7 * big + 120 * u);
            int y = cy - (7 * big + 120 * u) / 2 + 14 * u;
            textCentered(c, cx, y, "TENTACLE", big); y += 7 * big + 12 * u;
            const char* lines[] = {
                "THE BLOBS ARE COMING AROUND THE CORNERS.",
                "FEED THE MOUTH. GROW NEW MOUTHS. HOLD THE MAZE.",
                "1-4 GROW   DRAG SELECT   RIGHT CLICK MOVE",
                "E EVOLVE  U MOUTH  C COLONY  Q PULSE  H REPAIR",
            };
            for (const char* line : lines) { textCentered(c, cx, y, line, u); y += 11 * u; }
            y += 6 * u;
            if (std::fmod(clockSec, 1.0) < .65) textCentered(c, cx, y, "CLICK OR PRESS SPACE", u);
            return;
        }
        if (state_ == State::Lost) {
            panel(c, cx, cy, 360 * u, 96 * u);
            textCentered(c, cx, cy - 34 * u, "THE MOUTH IS SILENT", 2 * u);
            textCentered(c, cx, cy - 8 * u, "WAVE " + std::to_string(wave_) + "   KILLS " + std::to_string(kills_), u);
            textCentered(c, cx, cy + 16 * u, "R TO GROW AGAIN", u);
            return;
        }
        if (state_ == State::Won) {
            panel(c, cx, cy, 380 * u, 96 * u);
            textCentered(c, cx, cy - 34 * u, "THE TIDE BREAKS", 2 * u);
            textCentered(c, cx, cy - 8 * u, std::to_string(kills_) + " BLOBS EATEN", u);
            textCentered(c, cx, cy + 16 * u, "SPACE FOR ENDLESS   R TO RESTART", u);
            return;
        }
        if (paused_) {
            panel(c, cx, cy, 200 * u, 50 * u);
            textCentered(c, cx, cy - 14 * u, "PAUSED", 2 * u);
            textCentered(c, cx, cy + 6 * u, "P TO WAKE", u);
        }
        if (mark_ > 0) {
            const float mx = L.ox + markX_ * L.s, my = L.oy + markY_ * L.s;
            c.ring(mx, my, 3 + (0.6f - mark_) * 14, 1);
        }
    }
};

}  // namespace anaise
