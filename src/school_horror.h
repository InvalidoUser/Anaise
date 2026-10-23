#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace anaise {

// Session-only college horror simulation. All characters are fictional adults.
class SchoolHorror {
public:
    using Time = std::uint64_t;
    using Solid = bool (*)(int, int);
    struct Actor {
        double x = 0, y = 0, goalX = 0, goalY = 0;
        int health = 3;
        bool fleeing = false, escaped = false;
        Time hitAt = 0;
    };
    struct Blood { double x, y; Time when; unsigned seed; };
    std::array<Actor, 4> actors{};
    std::vector<Blood> blood;
    bool found = false, equipped = false, alarm = false;
    Time swingAt = 0;
    unsigned swings = 0;
    int lastVictim = -1;
    static constexpr double knifeX = 15.5, knifeY = 2.5;

    void reset() {
        *this = SchoolHorror{};
        const double xs[] = {5.5, 25.5, 5.5, 25.5}, ys[] = {7.5, 7.5, 17.5, 17.5};
        for (int i = 0; i < 4; ++i) {
            actors[i].x = actors[i].goalX = xs[i];
            actors[i].y = actors[i].goalY = ys[i];
        }
    }

    bool pickup(double x, double y) {
        if (found || std::hypot(x - knifeX, y - knifeY) > 1.15) return false;
        found = true; return true;
    }

    void toggle() { if (found) equipped = !equipped; }
    unsigned takeSounds() { const unsigned result = sounds_; sounds_ = 0; return result; }

    static bool visible(double x, double y, double tx, double ty, Solid solid) {
        const int steps = std::max(1, static_cast<int>(std::hypot(tx - x, ty - y) * 20));
        for (int i = 1; i <= steps; ++i) {
            const double f = static_cast<double>(i) / steps;
            if (solid(static_cast<int>(std::floor(x + (tx - x) * f)),
                      static_cast<int>(std::floor(y + (ty - y) * f)))) return false;
        }
        return true;
    }

    int attack(Time now, double x, double y, double angle, Solid solid) {
        if (!equipped || (swings && now - swingAt < 430)) return -1;
        swingAt = now; ++swings; sounds_ |= 2;
        int target = -1;
        double nearest = 1.55;
        for (int i = 0; i < 4; ++i) {
            const auto& a = actors[i];
            if (a.health == 0 || a.escaped) continue;
            const double dx = a.x - x, dy = a.y - y, d = std::hypot(dx, dy);
            if (d >= nearest || d < .01 || (dx * std::cos(angle) + dy * std::sin(angle)) / d < .55) continue;
            if (visible(x, y, a.x, a.y, solid)) { target = i; nearest = d; }
        }
        if (target < 0) return -1;
        auto& victim = actors[target];
        lastVictim = target;
        --victim.health; victim.hitAt = now; alarm = true;
        sounds_ |= 4 | (target & 1 ? 128u : 64u);
        blood.push_back(Blood{victim.x, victim.y, now, swings * 317u + static_cast<unsigned>(target)});
        if (blood.size() > 24) blood.erase(blood.begin());
        for (auto& a : actors)
            if (a.health && !a.escaped && std::hypot(a.x - victim.x, a.y - victim.y) < 8.0) a.fleeing = true;
        return target;
    }

    void tick(double elapsed, Time now, double px, double py, Solid solid) {
        const double dt = std::clamp(elapsed / 1000.0, 0.0, .12);
        if (dt == 0) return;
        // Both ends of the main hall are exits. Flood-fill once per tick so
        // frightened actors can choose a door rather than run into partitions.
        std::array<int, 31 * 25> distance;
        distance.fill(10000);
        std::vector<int> queue;
        for (int x : {1, 29}) { distance[12 * 31 + x] = 0; queue.push_back(12 * 31 + x); }
        const int dxs[] = {1, 0, -1, 0}, dys[] = {0, 1, 0, -1};
        for (std::size_t i = 0; i < queue.size(); ++i) {
            const int x = queue[i] % 31, y = queue[i] / 31;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + dxs[d], ny = y + dys[d];
                if (nx < 1 || nx > 29 || ny < 1 || ny > 23 || solid(nx, ny)) continue;
                const int index = ny * 31 + nx;
                if (distance[index] <= distance[queue[i]] + 1) continue;
                distance[index] = distance[queue[i]] + 1; queue.push_back(index);
            }
        }
        for (int i = 0; i < 4; ++i) {
            auto& a = actors[i];
            if (a.health == 0 || a.escaped) continue;
            if (!a.fleeing && equipped && std::hypot(a.x - px, a.y - py) < 4.5 && visible(px, py, a.x, a.y, solid)) {
                a.fleeing = true; alarm = true;
                sounds_ |= i & 1 ? 128u : 64u;
            }
            if (!a.fleeing) continue;
            if ((a.x < 2.1 || a.x > 28.9) && std::abs(a.y - 12.5) < .7) { a.escaped = true; continue; }
            if (std::hypot(a.goalX - a.x, a.goalY - a.y) < .03) {
                const int x = static_cast<int>(a.x), y = static_cast<int>(a.y);
                double best = 1e9;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + dxs[d], ny = y + dys[d];
                    if (nx < 1 || nx > 29 || ny < 1 || ny > 23 || solid(nx, ny)) continue;
                    const double separation = std::hypot(nx + .5 - px, ny + .5 - py);
                    const double score = distance[ny * 31 + nx] + (separation < 1.0 ? 12.0 : 0.0);
                    if (score < best) { best = score; a.goalX = nx + .5; a.goalY = ny + .5; }
                }
            }
            const double dx = a.goalX - a.x, dy = a.goalY - a.y, length = std::hypot(dx, dy);
            const double step = std::min(length, dt * (a.health == 3 ? 2.45 : 1.65));
            if (length > .001 && !(now > a.hitAt && now - a.hitAt < 240)) {
                a.x += dx / length * step; a.y += dy / length * step;
            }
        }
    }

    static bool selfTest() {
        const auto walls = +[](int x, int y) { return x <= 0 || y <= 0 || x >= 30 || y >= 24; };
        SchoolHorror h; h.reset();
        if (h.pickup(15.5, 12.5) || !h.pickup(15.5, 2.5) || h.pickup(15.5, 2.5)) return false;
        if (h.attack(2000, 5.5, 8.5, -1.57079632679, walls) != -1) return false;
        h.toggle();
        if (h.attack(2000, 5.5, 8.5, -1.57079632679, walls) != 0 || h.actors[0].health != 2) return false;
        h.attack(2100, 5.5, 8.5, -1.57079632679, walls);
        if (h.actors[0].health != 2 || !h.actors[0].fleeing || !h.alarm) return false;
        h.attack(2500, 5.5, 8.5, -1.57079632679, walls);
        h.attack(3000, 5.5, 8.5, -1.57079632679, walls);
        if (h.actors[0].health != 0 || h.blood.size() != 3 || !(h.takeSounds() & 64)) return false;
        h.tick(100, 4000, 15.5, 12.5, walls);
        if (h.actors[0].x != 5.5 || h.actors[0].y != 7.5) return false;
        h.reset(); h.found = true; h.toggle();
        h.tick(120, 4000, 5.5, 8.5, walls);
        if (!h.actors[0].fleeing || std::hypot(h.actors[0].x - 5.5, h.actors[0].y - 7.5) < .1) return false;
        const auto barrier = +[](int x, int y) { return x <= 0 || y <= 0 || x >= 30 || y >= 24 || y == 8; };
        h.reset(); h.found = true; h.toggle();
        if (h.attack(2000, 5.5, 9.0, -1.57079632679, barrier) != -1) return false;
        return true;
    }

private:
    unsigned sounds_ = 0;
};
} // namespace anaise
