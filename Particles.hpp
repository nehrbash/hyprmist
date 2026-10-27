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
    double age(double now) const { return std::clamp((now - born) / life, 0.0, 1.0); }
    Vec position(double now) const {
        double t = std::max(0.0, now - born);
        return origin + velocity * ((1.0 - std::exp(-3.0 * t)) / 3.0) + Vec{0, -42 * t - 22 * t * t};
    }
    Box bounds(double now) const {
        Vec p = position(now);
        double r = radius * (1.0 + 0.85 * age(now));
        return {p.x - r, p.y - r, 2 * r, 2 * r};
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
    void emit(Vec at, Vec velocity, double now, double radius, double opacity) {
        if (puffs.size() >= limit) return;
        double speed = velocity.length();
        if (speed > 1200) velocity = velocity * (1200 / speed);
        puffs.push_back({at, velocity * 0.20 + Vec{(random() - 0.5) * 45, 0}, now,
                        0.65 + random() * 0.35, radius * (0.8 + random() * 0.4), opacity,
                        unsigned(random() * 4)});
    }
    void burst(Box b, double now) {
        unsigned count = std::clamp(unsigned((b.w + b.h) / 18), 24u, 90u);
        for (unsigned i = 0; i < count; ++i) {
            Vec p{b.x + random() * b.w, b.y + random() * b.h};
            if (i % 3 != 0) {
                unsigned side = unsigned(random() * 4);
                if (side < 2) p.x = b.x + side * b.w;
                else p.y = b.y + (side - 2) * b.h;
            }
            emit(p, {}, now, 70, 0.42);
        }
    }
    void trail(Box b, Vec delta, double dt, double now, double opacity, double& credit) {
        double distance = delta.length();
        if (!std::isfinite(distance) || distance <= 0 || distance > 500 || !std::isfinite(dt) || dt <= 0) return;
        credit += distance / 8;
        unsigned count = std::min(24u, unsigned(credit));
        credit -= std::floor(credit);
        for (unsigned i = 0; i < count; ++i) {
            Vec p{b.x + random() * b.w, b.y + random() * b.h};
            if (std::abs(delta.x) > std::abs(delta.y)) p.x = b.x + (delta.x > 0 ? 0 : b.w);
            else p.y = b.y + (delta.y > 0 ? 0 : b.h);
            p = p - delta * random();
            emit(p, delta * (1.0 / dt), now, 52, opacity);
        }
    }
    void prune(double now) {
        std::erase_if(puffs, [now](const Puff& p) { return now >= p.born + p.life; });
    }
};
}
