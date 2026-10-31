#pragma once
#include <array>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace Mist {
class Palette {
  public:
    using Color = std::array<float, 3>;
    using Colors = std::array<Color, 5>;
    static constexpr Colors defaults{{{.18f,.20f,.24f}, {.9f,.9f,.9f},
                                      {.65f,.76f,.85f}, {.75f,.68f,.85f}, {.85f,.74f,.55f}}};
    Colors colors = defaults;
    static constexpr std::array<const char*, 5> names{"shadow","light","primary","secondary","tertiary"};
    static constexpr std::array<const char*, 5> roles{"surfaceContainerHighest","onSurface","primary","secondary","tertiary"};
    static std::optional<Color> parse(std::string value) {
        if (value.starts_with('#')) value.erase(0,1);
        if (value.size()!=6) return {};
        unsigned rgb=0;
        auto result=std::from_chars(value.data(),value.data()+value.size(),rgb,16);
        if (result.ec!=std::errc{} || result.ptr!=value.data()+value.size()) return {};
        return Color{float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255};
    }
    static std::filesystem::path xdg(const char* variable, const char* fallback) {
        if (const auto* path=std::getenv(variable); path && *path) return path;
        const auto* home=std::getenv("HOME");
        return std::filesystem::path(home ? home : "")/fallback;
    }
    Palette(std::filesystem::path scheme=xdg("XDG_STATE_HOME",".local/state")/"burl/scheme.json",
            std::filesystem::path options=xdg("XDG_CONFIG_HOME",".config")/"hyprmist/colors.json")
        : schemePath(std::move(scheme)), optionsPath(std::move(options)) {}
    void refresh(double now, bool force=false) {
        if (!force && now-lastCheck<.5) return;
        lastCheck=now;
        bool changed=read(schemePath,schemeStamp,scheme,"colours");
        changed=read(optionsPath,optionsStamp,options,"colors") || changed;
        if (!changed && !force) return;
        for (size_t i=0;i<colors.size();++i) {
            std::string role=roles[i];
            if (options.contains(names[i]) && options[names[i]].is_string()) {
                const auto value=options[names[i]].get<std::string>();
                if (auto explicitColor=parse(value)) { colors[i]=*explicitColor; continue; }
                role=value;
            }
            colors[i]=defaults[i];
            if (scheme.contains(role) && scheme[role].is_string())
                if (auto color=parse(scheme[role].get<std::string>())) colors[i]=*color;
        }
    }
  private:
    std::filesystem::path schemePath,optionsPath;
    std::optional<std::filesystem::file_time_type> schemeStamp,optionsStamp;
    nlohmann::json scheme=nlohmann::json::object(),options=nlohmann::json::object();
    double lastCheck=-1;
    static bool read(const std::filesystem::path& path, std::optional<std::filesystem::file_time_type>& stamp,
                     nlohmann::json& value, const char* key) {
        std::error_code error;
        auto current=std::filesystem::last_write_time(path,error);
        if (error) {
            if (!stamp) return false;
            stamp.reset(); value=nlohmann::json::object(); return true;
        }
        if (stamp && *stamp==current) return false;
        try {
            std::ifstream input(path);
            auto document=nlohmann::json::parse(input);
            if (!document.contains(key) || !document[key].is_object()) return false;
            value=document[key]; stamp=current; return true;
        } catch (const nlohmann::json::exception&) { return false; }
    }
};
}
