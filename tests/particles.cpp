#include "../Particles.hpp"
#include <cassert>
#include <limits>
int main() {
    Mist::Puff p{{0, 0}, {200, 0}, 0, 1, 50, .5, 0};
    assert(p.position(.5).x > 0 && p.position(.5).y < 0);
    assert(p.bounds(.5).w > p.bounds(0).w);
    double previous = p.alpha(.1);
    for (int i = 11; i <= 100; ++i) {
        double alpha = p.alpha(i / 100.0);
        assert(alpha <= previous);
        previous = alpha;
    }
    Mist::Particles s;
    double credit = 0;
    s.trail({100, 200, 400, 300}, {20, 0}, .02, 0, .3, credit);
    assert(!s.puffs.empty());
    for (const auto& q : s.puffs) {
        assert(q.origin.x <= 100 && q.velocity.x > 0);
        assert(q.bounds(.1).x < 100);
    }
    Mist::Particles slow, fast;
    double a = 0, b = 0;
    for (int i = 0; i < 60; ++i) slow.trail({0, 0, 100, 100}, {4, 0}, 1.0 / 60, 0, .3, a);
    for (int i = 0; i < 240; ++i) fast.trail({0, 0, 100, 100}, {1, 0}, 1.0 / 240, 0, .3, b);
    assert(slow.puffs.size() == fast.puffs.size());
    Mist::Particles subpixel;
    double c = 0;
    for (int i = 0; i < 960; ++i) subpixel.trail({0, 0, 100, 100}, {.25, 0}, 1.0 / 960, 0, .3, c);
    assert(subpixel.puffs.size() == slow.puffs.size());
    Mist::Particles cloud;
    double cloudCredit = 0;
    cloud.trail({100, 200, 400, 900}, {20, 0}, .02, 0, .65, cloudCredit, true);
    assert(cloud.puffs.size() > s.puffs.size());
    for (const auto& q : cloud.puffs) {
        assert(q.bounds(0).w > q.bounds(0).h);
        assert(q.position(.5).y < q.origin.y);
    }
    auto count = s.puffs.size();
    s.trail({100, 200, 400, 300}, {}, .02, .1, .3, credit);
    assert(s.puffs.size() == count);
    s.trail({100, 200, 400, 300}, {std::numeric_limits<double>::quiet_NaN(), 0}, .02, .1, .3, credit);
    s.trail({100, 200, 400, 300}, {20, 0}, std::numeric_limits<double>::infinity(), .1, .3, credit);
    assert(s.puffs.size() == count);
    for (int i = 0; i < 100; ++i) s.burst({0, 0, 800, 600}, 0);
    assert(s.puffs.size() <= Mist::Particles::limit);
    s.prune(2);
    assert(s.puffs.empty());
}
