#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <lua.hpp>
#include <drm_fourcc.h>
#include <array>
#include <chrono>
#include <unordered_map>
#include "Particles.hpp"

namespace {
HANDLE handle;
Mist::Particles particles;
std::vector<CHyprSignalListener> listeners;
std::array<SP<Render::ITexture>, 4> textures;
struct Sample { Mist::Box box; double time; bool visible; double quietUntil = 0; double credit = 0; };
std::unordered_map<Desktop::View::CWindow*, Sample> samples;
bool enabled = true;
double lastTick = 0;
const auto epoch = std::chrono::steady_clock::now();
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch).count(); }
CBox box(const Mist::Box& b) { return {b.x, b.y, b.w, b.h}; }
Mist::Box windowBox(PHLWINDOW w, bool goal = false) {
    auto p = goal ? w->positionAnimation()->goal() : w->positionAnimation()->value();
    auto s = goal ? w->sizeAnimation()->goal() : w->sizeAnimation()->value();
    if (w->m_workspace && !w->m_pinned) p += w->m_workspace->m_renderOffset->value();
    return {p.x, p.y, s.x, s.y};
}
bool visible(PHLWINDOW w) {
    return w && w->m_isMapped && w->visible() && !Fullscreen::controller()->isFullscreen(w)
        && g_pHyprRenderer->shouldRenderWindow(w);
}
void damage(double now) {
    for (const auto& p : particles.puffs) g_pHyprRenderer->damageBox(box(p.bounds(now)));
}
void clear() {
    damage(seconds());
    particles.puffs.clear();
    samples.clear();
}
float hash(int x, int y, unsigned seed) {
    uint32_t n = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + seed * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return float(n ^ (n >> 16)) / 4294967296.0f;
}
float noise(float x, float y, unsigned seed) {
    int ix = std::floor(x), iy = std::floor(y);
    float a = x - ix, b = y - iy;
    a = a * a * (3 - 2 * a); b = b * b * (3 - 2 * b);
    return std::lerp(std::lerp(hash(ix, iy, seed), hash(ix + 1, iy, seed), a),
                     std::lerp(hash(ix, iy + 1, seed), hash(ix + 1, iy + 1, seed), a), b);
}
void ensureTextures() {
    if (textures[0]) return;
    constexpr int N = 128;
    for (unsigned variant = 0; variant < textures.size(); ++variant) {
        std::vector<uint8_t> pixels(N * N * 4);
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            float u = (x + 0.5f) / N * 2 - 1, v = (y + 0.5f) / N * 2 - 1;
            float density = 0, weight = 0.55f, frequency = 3;
            for (int octave = 0; octave < 5; ++octave) {
                density += weight * noise(u * frequency + 7, v * frequency + 9, variant + 1);
                weight *= 0.5f; frequency *= 2;
            }
            float warpX = noise(u * 2 + 13, v * 2 + 3, variant + 19) - 0.5f;
            float warpY = noise(u * 2 + 5, v * 2 + 17, variant + 31) - 0.5f;
            float r = std::hypot(u + 0.8f * warpX, v + 0.8f * warpY);
            float edge = std::clamp((1 - r) / 0.65f, 0.0f, 1.0f);
            edge *= std::clamp((1 - std::max(std::abs(u), std::abs(v))) / 0.18f, 0.0f, 1.0f);
            edge = edge * edge * (3 - 2 * edge);
            float alpha = edge * std::clamp((density - 0.12f) * 1.5f, 0.0f, 1.0f);
            float shade = 0.22f + density * 0.38f;
            auto i = (y * N + x) * 4;
            pixels[i] = uint8_t(255 * (shade + 0.025f) * alpha);
            pixels[i + 1] = uint8_t(255 * shade * alpha);
            pixels[i + 2] = uint8_t(255 * (shade + 0.045f) * alpha);
            pixels[i + 3] = uint8_t(255 * alpha);
        }
        textures[variant] = g_pHyprRenderer->createTexture(DRM_FORMAT_ABGR8888, pixels.data(), N * 4, {N, N});
    }
}
class PuffElement : public CTexPassElement {
  public:
    using CTexPassElement::CTexPassElement;
    const char* passName() override { return "hyprmist"; }
};
void tick() {
    double now = seconds();
    if (now - lastTick < 0.001) return;
    if (!enabled || g_pSessionLockManager->isSessionLocked()) { clear(); lastTick = now; return; }
    damage(lastTick);
    particles.prune(now);
    damage(now);
    lastTick = now;
}
void sampleWindows() {
    if (!enabled || g_pSessionLockManager->isSessionLocked()) return;
    double now = seconds();
    for (auto& w : Desktop::windowState()->windows()) {
        if (!w || !w->m_isMapped) continue;
        auto b = windowBox(w);
        bool shown = visible(w);
        auto it = samples.find(w.get());
        if (it != samples.end() && shown && it->second.visible) {
            double dt = now - it->second.time;
            if (dt < 0.001) continue;
            if (dt < 0.1 && now > it->second.quietUntil) {
                Mist::Vec delta{b.x - it->second.box.x, b.y - it->second.box.y};
                bool workspace = w->m_workspace && w->m_workspace->m_renderOffset->isBeingAnimated();
                particles.trail(b, delta, dt, now, workspace ? 0.65 : 0.55, it->second.credit, workspace);
            }
        }
        double quiet = it == samples.end() ? 0 : it->second.quietUntil;
        double credit = it == samples.end() ? 0 : it->second.credit;
        samples[w.get()] = {b, now, shown, quiet, credit};
    }
    damage(now);
}
void render() {
    if (!enabled || particles.puffs.empty() || g_pSessionLockManager->isSessionLocked()) return;
    auto monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
    if (!monitor) return;
    ensureTextures();
    double now = seconds();
    for (const auto& p : particles.puffs) {
        auto b = p.bounds(now);
        CBox local{b.x - monitor->m_position.x, b.y - monitor->m_position.y, b.w, b.h};
        if (local.x + local.w < 0 || local.y + local.h < 0 || local.x > monitor->m_size.x || local.y > monitor->m_size.y) continue;
        CTexPassElement::SRenderData data;
        data.tex = textures[p.texture];
        data.box = local.scale(monitor->m_scale);
        data.a = p.alpha(now);
        g_pHyprRenderer->m_renderPass.add(makeUnique<PuffElement>(std::move(data)));
    }
}
int setEnabled(lua_State* L) {
    enabled = lua_toboolean(L, 1);
    if (!enabled) clear();
    return 0;
}
int stats(lua_State* L) {
    lua_pushinteger(L, particles.puffs.size());
    return 1;
}
int trackedWindows(lua_State* L) {
    lua_pushinteger(L, samples.size());
    return 1;
}
}
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE h) {
    handle = h;
    if (HyprlandAPI::getHyprlandVersion(h).hash != "efb50993780079460b0cbed1363e2166a2de1d9f")
        throw std::runtime_error("hyprmist requires Hyprland 0.56.2 (efb50993)");
    listeners.push_back(Event::bus()->m_events.render.preChecks.listen([](PHLMONITOR) { tick(); }));
    listeners.push_back(Event::bus()->m_events.render.pre.listen([](PHLMONITOR) { sampleWindows(); }));
    listeners.push_back(Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        if (stage == RENDER_POST_WINDOWS) render();
    }));
    listeners.push_back(Event::bus()->m_events.window.openLate.listen([](PHLWINDOW w) {
        if (!enabled || g_pSessionLockManager->isSessionLocked() || !visible(w)) return;
        auto b = windowBox(w, true);
        double now = seconds();
        samples[w.get()] = {b, now, true, now + 0.65, 0};
        particles.burst(b, now);
        damage(seconds());
    }));
    listeners.push_back(Event::bus()->m_events.window.close.listen([](PHLWINDOW w) {
        auto it = samples.find(w.get());
        if (enabled && !g_pSessionLockManager->isSessionLocked() && it != samples.end() && it->second.visible) {
            particles.burst(it->second.box, seconds());
            damage(seconds());
        }
        samples.erase(w.get());
    }));
    listeners.push_back(Event::bus()->m_events.window.destroy.listen([](PHLWINDOWREF w) { samples.erase(w.get()); }));
    HyprlandAPI::addLuaFunction(h, "hyprmist", "enable", setEnabled);
    HyprlandAPI::addLuaFunction(h, "hyprmist", "particles", stats);
    HyprlandAPI::addLuaFunction(h, "hyprmist", "tracked_windows", trackedWindows);
    return {"hyprmist", "Expanding smoke beyond window borders", "nehrbash", "0.1.0"};
}
APICALL EXPORT void PLUGIN_EXIT() {
    listeners.clear();
    clear();
    g_pHyprRenderer->m_renderPass.removeAllOfType("hyprmist");
    g_pHyprRenderer->glBackend()->makeEGLCurrent();
    textures = {};
}
