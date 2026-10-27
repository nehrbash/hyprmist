#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FadingOutState.hpp>
#include <hyprland/src/desktop/state/WindowFadeout.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/render/transformer/Transformer.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <lua.hpp>
#include <drm_fourcc.h>
#include <chrono>
#include <memory>
#include <unordered_map>
#include "CloudRenderer.hpp"

namespace {
Mist::Particles particles;
Mist::CloudRenderer clouds;
std::vector<CHyprSignalListener> listeners;
std::array<SP<Render::ITexture>, 4> textures;
bool enabled = true;
double lastTick = 0;
const auto epoch = std::chrono::steady_clock::now();
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch).count(); }
CBox box(const Mist::Box& b) { return {b.x, b.y, b.w, b.h}; }
Mist::Box windowBox(PHLWINDOW w) {
    auto p = w->positionAnimation()->value();
    auto s = w->sizeAnimation()->value();
    if (w->m_workspace && !w->m_pinned) p += w->m_workspace->m_renderOffset->value();
    return {p.x, p.y, s.x, s.y};
}
Mist::Box monitorBox(PHLMONITOR m) {
    return {m->m_position.x,m->m_position.y,m->m_transformedSize.x/m->m_scale,m->m_transformedSize.y/m->m_scale};
}
bool visible(PHLWINDOW w) {
    return w && w->m_isMapped && !w->isHidden() && !Fullscreen::controller()->isFullscreen(w)
        && g_pHyprRenderer->shouldRenderWindow(w);
}
void damageCloud(const Mist::Cloud& c) {
    auto b=box(c.box); b.expand(165); g_pHyprRenderer->damageBox(b);
}
class VeilTransformer : public Render::IWindowTransformer {
    std::shared_ptr<Mist::Cloud> cloud;
    // Blur mattes and window content must retain separate transformed framebuffers.
    std::unordered_map<Render::IFramebuffer*,SP<Render::IFramebuffer>> outputs;
  public:
    explicit VeilTransformer(std::shared_ptr<Mist::Cloud> c) : cloud(std::move(c)) {}
    SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in) override {
        auto monitor=g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (!enabled || !cloud->active || !monitor || g_pHyprRenderer->m_bRenderingSnapshot) return in;
        return clouds.mask(in,outputs[in.get()],monitorBox(monitor),*cloud);
    }
};
struct Sample {
    Mist::Box box{};
    double time=0, opened=-1, credit=0;
    bool visible=false;
    std::shared_ptr<Mist::Cloud> cloud=std::make_shared<Mist::Cloud>();
    VeilTransformer* transformer=nullptr;
};
std::unordered_map<Desktop::View::CWindow*,Sample> samples;
struct PendingClose { Mist::Box box; PHLMONITORREF monitor; PHLWORKSPACEREF workspace; double time,seed; };
std::vector<PendingClose> pending;
struct Closing {
    WP<Desktop::IFadeout> fadeout;
    SP<Render::IFramebuffer> original;
    Mist::Cloud cloud;
    double born;
    float initialAlpha;
};
std::unordered_map<Desktop::IFadeout*,Closing> closing;
void removeTransformer(PHLWINDOW w, Sample& s) {
    if (!s.transformer) return;
    std::erase_if(w->m_transformers,[&](auto& t) { return t.get()==s.transformer; });
    s.transformer=nullptr;
    g_pHyprRenderer->damageWindow(w);
}
void damageParticles(double now) {
    for (const auto& p:particles.puffs) g_pHyprRenderer->damageBox(box(p.bounds(now)));
}
void clear() {
    for (auto& w:Desktop::windowState()->windows()) {
        auto it=samples.find(w.get());
        if(it!=samples.end()) { damageCloud(*it->second.cloud); removeTransformer(w,it->second); }
    }
    for(auto& [key,c]:closing) {
        if(auto f=c.fadeout.lock()) { auto target=f->framebuffer(); clouds.copy(c.original,target); }
        damageCloud(c.cloud);
    }
    damageParticles(seconds());
    particles.puffs.clear(); samples.clear(); closing.clear(); pending.clear();
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
    double now=seconds();
    if(now-lastTick<.001) return;
    damageParticles(lastTick); particles.prune(now); damageParticles(now);
    for(auto& [w,s]:samples) if(s.cloud->active) damageCloud(*s.cloud);
    for(auto& [f,c]:closing) damageCloud(c.cloud);
    std::erase_if(pending,[now](const auto& p) { return now-p.time>1; });
    lastTick=now;
}
void sampleWindows() {
    if(!enabled || g_pSessionLockManager->isSessionLocked()) return;
    double now=seconds();
    for(auto& w:Desktop::windowState()->windows()) {
        if(!w || !w->m_isMapped) continue;
        auto [it,inserted]=samples.try_emplace(w.get());
        auto& s=it->second;
        auto b=windowBox(w);
        bool shown=visible(w);
        auto& c=*s.cloud;
        if(inserted) c.seed=particles.random();
        bool wasActive=c.active;
        c.active=false;
        c.box=b; c.stamp=now;
        auto workspace=w->m_workspace;
        if(shown && workspace && workspace->m_renderOffset->isBeingAnimated() && !w->m_pinned) {
            auto& anim=workspace->m_renderOffset;
            auto delta=anim->goal()-anim->begun();
            double length=delta.size();
            if(length>1) {
                c.active=true;
                c.direction={delta.x/length,delta.y/length};
                c.progress=std::clamp(double(anim->getCurveValue()),0.0,1.0);
                c.incoming=anim->goal().size()<1;
                c.elapsed=anim->getPercent()*.8;
            }
        } else if(shown && s.opened>=0 && now-s.opened<.48) {
            c.active=true; c.incoming=true; c.direction={0,-1};
            c.progress=(now-s.opened)/.48; c.elapsed=now-s.opened;
        } else if(shown && s.visible && !inserted && now-s.time>.001 && now-s.time<.1) {
            particles.trail(b,{b.x-s.box.x,b.y-s.box.y},now-s.time,now,.25,s.credit);
        }
        if(c.active && !s.transformer) {
            auto transformer=makeUnique<VeilTransformer>(s.cloud);
            s.transformer=transformer.get();
            w->m_transformers.emplace_back(std::move(transformer));
        } else if(!c.active) {
            removeTransformer(w,s);
            c.field.reset(); c.smoke.reset(); c.prepared=-1;
        }
        if(c.active || wasActive) damageCloud(c);
        s.box=b; s.time=now; s.visible=shown;
    }
    damageParticles(now);
}
void updateClosing() {
    const double now=seconds();
    for(auto& f:Desktop::fadingOutState()->fadeouts()) {
        if(closing.size()>=8) break;
        if(!dynamic_cast<Desktop::CWindowFadeout*>(f.get()) || closing.contains(f.get()) || !f->framebuffer()) continue;
        auto p=std::find_if(pending.begin(),pending.end(),[&](const auto& p) {return p.monitor==f->monitor() && p.workspace==f->workspace();});
        if(p==pending.end()) continue;
        Closing c;
        c.fadeout=f; c.born=now; c.initialAlpha=std::max(.001f,f->alpha());
        c.cloud.box=p->box; c.cloud.seed=p->seed; c.cloud.incoming=false; c.cloud.active=true;
        clouds.copy(f->framebuffer(),c.original);
        closing.emplace(f.get(),std::move(c)); pending.erase(p);
    }
    std::erase_if(closing,[](auto& entry) { return entry.second.fadeout.expired(); });
    for(auto& [key,c]:closing) {
        auto f=c.fadeout.lock();
        auto m=f ? f->monitor().lock() : nullptr;
        if(!m) continue;
        c.cloud.progress=std::clamp(1.0-double(f->alpha()/c.initialAlpha),0.0,1.0);
        c.cloud.elapsed=now-c.born; c.cloud.stamp=now;
        auto target=f->framebuffer();
        clouds.mask(c.original,target,monitorBox(m),c.cloud);
    }
}
void addCloud(Mist::Cloud& c, PHLMONITOR monitor, Desktop::IFadeout* fadeout=nullptr) {
    clouds.prepare(c);
    CBox local=box(c.domain);
    local.translate(-monitor->m_position);
    local.scale(monitor->m_scale);
    if(fadeout) {
        auto rb=fadeout->renderBox();
        local.x=rb.x+local.x*rb.w/monitor->m_transformedSize.x;
        local.y=rb.y+local.y*rb.h/monitor->m_transformedSize.y;
        local.w*=rb.w/monitor->m_transformedSize.x;
        local.h*=rb.h/monitor->m_transformedSize.y;
    }
    CTexPassElement::SRenderData data;
    data.tex=c.smoke->getTexture(); data.box=local; data.flipEndFrame=true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<PuffElement>(std::move(data)));
}
void render() {
    auto monitor=g_pHyprRenderer->m_renderData.pMonitor.lock();
    if(!monitor) return;
    if(!enabled || g_pSessionLockManager->isSessionLocked()) { clear(); return; }
    updateClosing();
    for(auto& [w,s]:samples) {
        if(!s.cloud->active) continue;
        CBox bounds=box(s.cloud->box); bounds.expand(165);
        if(bounds.x+bounds.w<monitor->m_position.x || bounds.y+bounds.h<monitor->m_position.y ||
           bounds.x>monitor->m_position.x+monitor->m_size.x || bounds.y>monitor->m_position.y+monitor->m_size.y) continue;
        addCloud(*s.cloud,monitor);
    }
    for(auto& [key,c]:closing) {
        auto f=c.fadeout.lock();
        if(f && f->monitor()==monitor) addCloud(c.cloud,monitor,f.get());
    }
    if(particles.puffs.empty()) return;
    ensureTextures();
    double now=seconds();
    for(const auto& p:particles.puffs) {
        auto b=p.bounds(now);
        CBox local{b.x-monitor->m_position.x,b.y-monitor->m_position.y,b.w,b.h};
        CTexPassElement::SRenderData data;
        data.tex=textures[p.texture]; data.box=local.scale(monitor->m_scale); data.a=p.alpha(now);
        g_pHyprRenderer->m_renderPass.add(makeUnique<PuffElement>(std::move(data)));
    }
}
int setEnabled(lua_State* L) {
    enabled=lua_toboolean(L,1);
    if(!enabled) {g_pHyprRenderer->glBackend()->makeEGLCurrent(); clear();}
    return 0;
}
int stats(lua_State* L) { lua_pushinteger(L,particles.puffs.size()); return 1; }
int activeEffects(lua_State* L) {
    size_t count=particles.puffs.size()+closing.size();
    for(const auto& [w,s]:samples) count+=s.cloud->active;
    lua_pushinteger(L,count); return 1;
}
int trackedWindows(lua_State* L) { lua_pushinteger(L,samples.size()); return 1; }
int closingWindows(lua_State* L) { lua_pushinteger(L,closing.size()); return 1; }
int verifyField(lua_State* L) {
    g_pHyprRenderer->glBackend()->makeEGLCurrent();
    lua_pushboolean(L,clouds.verifyField()); return 1;
}
}
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE h) {
    if(HyprlandAPI::getHyprlandVersion(h).hash!="efb50993780079460b0cbed1363e2166a2de1d9f")
        throw std::runtime_error("hyprmist requires Hyprland 0.56.2 (efb50993)");
    g_pHyprRenderer->glBackend()->makeEGLCurrent(); clouds.init();
    listeners.push_back(Event::bus()->m_events.render.preChecks.listen([](PHLMONITOR) {tick();}));
    listeners.push_back(Event::bus()->m_events.render.pre.listen([](PHLMONITOR) {sampleWindows();}));
    listeners.push_back(Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {if(stage==RENDER_POST_WINDOWS) render();}));
    listeners.push_back(Event::bus()->m_events.window.openLate.listen([](PHLWINDOW w) {
        if(!enabled) return;
        auto& s=samples[w.get()]; s.opened=seconds(); s.cloud->seed=particles.random();
    }));
    listeners.push_back(Event::bus()->m_events.window.close.listen([](PHLWINDOW w) {
        auto it=samples.find(w.get());
        if(it==samples.end()) return;
        auto& s=it->second;
        if(enabled && !g_pSessionLockManager->isSessionLocked() && s.visible)
            pending.push_back({s.box,w->m_monitor,w->m_workspace,seconds(),s.cloud->seed});
        removeTransformer(w,s); damageCloud(*s.cloud); samples.erase(it);
    }));
    listeners.push_back(Event::bus()->m_events.window.destroy.listen([](PHLWINDOWREF w) {samples.erase(w.get());}));
    HyprlandAPI::addLuaFunction(h,"hyprmist","enable",setEnabled);
    HyprlandAPI::addLuaFunction(h,"hyprmist","particles",stats);
    HyprlandAPI::addLuaFunction(h,"hyprmist","active_effects",activeEffects);
    HyprlandAPI::addLuaFunction(h,"hyprmist","tracked_windows",trackedWindows);
    HyprlandAPI::addLuaFunction(h,"hyprmist","closing_windows",closingWindows);
    HyprlandAPI::addLuaFunction(h,"hyprmist","verify_field",verifyField);
    return {"hyprmist","Coherent window reveals and rolling smoke","nehrbash","0.2.0"};
}
APICALL EXPORT void PLUGIN_EXIT() {
    listeners.clear();
    g_pHyprRenderer->glBackend()->makeEGLCurrent();
    clear();
    g_pHyprRenderer->m_renderPass.removeAllOfType("hyprmist");
    textures={}; clouds.release();
}
