#include "../Palette.hpp"
#include <cassert>
#include <iostream>
#include <unistd.h>

int main() {
    char pattern[]="/tmp/hyprmist-palette-XXXXXX";
    const std::filesystem::path dir=mkdtemp(pattern);
    auto write=[&](const char* name,const char* text) {
        std::ofstream(dir/name)<<text;
    };
    write("scheme.json",R"({"colours":{"primary":"c7ae7b","secondary":"aebdcb","tertiary":"baa4d0"}})");
    Mist::Palette palette(dir/"scheme.json",dir/"colors.json");
    palette.refresh(0,true);
    assert(palette.colors[2]==*Mist::Palette::parse("c7ae7b"));
    write("colors.json",R"({"colors":{"primary":"#ff0000","secondary":"tertiary"}})");
    palette.refresh(1);
    assert(palette.colors[2]==*Mist::Palette::parse("ff0000"));
    assert(palette.colors[3]==*Mist::Palette::parse("baa4d0"));
    write("scheme.json",R"({"colours":{"primary":"00ff00","tertiary":"0011ff"}})");
    palette.refresh(2);
    assert(palette.colors[2]==*Mist::Palette::parse("ff0000"));
    assert(palette.colors[3]==*Mist::Palette::parse("0011ff"));
    write("scheme.json","{");
    palette.refresh(3);
    assert(palette.colors[3]==*Mist::Palette::parse("0011ff"));
    std::filesystem::remove(dir/"colors.json");
    palette.refresh(4);
    assert(palette.colors[2]==*Mist::Palette::parse("00ff00"));
    assert(!Mist::Palette::parse("#12345g"));
    assert(!Mist::Palette::parse("#123"));
    std::filesystem::remove_all(dir);
    std::cout<<"PASS: palette follows theme updates, preserves overrides and survives partial writes\n";
}
