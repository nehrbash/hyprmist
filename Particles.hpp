#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Mist {
struct Vec {
    double x = 0, y = 0;
    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(double s) const { return {x * s, y * s}; }
    double length() const { return std::hypot(x, y); }
};
struct Box { double x, y, w, h; };
struct Puff {
    Vec origin, velocity;
    double born, life, radius, opacity;
    unsigned texture;
    Vec stretch{1, 1};
    double phase = 0;
    double age(double now) const { return std::clamp((now - born) / life, 0.0, 1.0); }
    Vec position(double now) const {
        double t = std::max(0.0, now - born);
        return origin + velocity * (0.25 * t + 0.25 * (1.0 - std::exp(-3.0 * t)))
            + Vec{18 * (std::sin(phase + 3 * t) - std::sin(phase)),
                  -58 * t - 28 * t * t + 10 * (std::cos(phase + 3 * t) - std::cos(phase))};
    }
    Box bounds(double now) const {
        Vec p = position(now);
        double r = radius * (1.0 + 0.85 * age(now));
        return {p.x - r * stretch.x, p.y - r * stretch.y, 2 * r * stretch.x, 2 * r * stretch.y};
    }
    double alpha(double now) const {
        double t = age(now);
        double in = std::min(t / 0.10, 1.0);
        double out = (1.0 - t) * (1.0 - t);
        return opacity * in * out;
    }
};
class Particles {
    uint32_t state = 0x739a28f1;
  public:
    static constexpr size_t limit = 600;
    std::vector<Puff> puffs;
    double random() {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return double(state) / 4294967296.0;
    }
    void emit(Vec at, Vec velocity, double now, double radius, double opacity, Vec stretch = {1, 1}) {
        if (puffs.size() >= limit) return;
        double speed = velocity.length();
        if (speed > 1200) velocity = velocity * (1200 / speed);
        puffs.push_back({at, velocity * 0.20 + Vec{(random() - 0.5) * 45, 0}, now,
                        0.7 + random() * 0.3, radius * (0.8 + random() * 0.4), opacity,
                        unsigned(random() * 4),
                        {stretch.x * (0.8 + random() * 0.5), stretch.y * (0.8 + random() * 0.5)}, random() * 6.283185307});
    }
    void burst(Box b, double now) {
        unsigned count = std::clamp(unsigned(b.w * b.h / 16000), 24u, 90u);
        unsigned columns = std::max(1u, unsigned(std::ceil(std::sqrt(count * b.w / std::max(b.h, 1.0)))));
        unsigned rows = (count + columns - 1) / columns;
        double radius = std::clamp(std::min(b.w, b.h) * 0.25, 100.0, 210.0);
        for (unsigned i = 0; i < count; ++i) {
            Vec p{b.x + (i % columns + random()) / columns * b.w,
                  b.y + (i / columns + random()) / rows * b.h};
            emit(p, {}, now, radius, 0.8);
        }
    }
    void trail(Box b, Vec delta, double dt, double now, double opacity, double& credit, bool cloud = false) {
        double distance = delta.length();
        if (!std::isfinite(distance) || distance <= 0 || distance > 500 || !std::isfinite(dt) || dt <= 0) return;
        bool horizontal = std::abs(delta.x) > std::abs(delta.y);
        double coverage = std::sqrt(std::max(0.0, b.w * b.h)) / 160;
        credit += distance / 8 * (cloud ? std::max(1.0, coverage) : 1.0);
        unsigned count = std::min(24u, unsigned(credit));
        credit -= std::floor(credit);
        for (unsigned i = 0; i < count; ++i) {
            Vec p{b.x + random() * b.w, b.y + random() * b.h};
            if (!cloud) {
                if (horizontal) p.x = b.x + (delta.x > 0 ? 0 : b.w);
                else p.y = b.y + (delta.y > 0 ? 0 : b.h);
            }
            p = p - delta * random();
            Vec stretch = cloud ? (horizontal ? Vec{1.5, 1.0} : Vec{1.0, 1.5}) : Vec{1, 1};
            double radius = cloud ? std::clamp(std::min(b.w, b.h) * 0.25, 120.0, 210.0) : 72;
            emit(p, delta * (1.0 / dt), now, radius, opacity, stretch);
        }
    }
    void prune(double now) {
        std::erase_if(puffs, [now](const Puff& p) { return now >= p.born + p.life; });
    }
};
}
