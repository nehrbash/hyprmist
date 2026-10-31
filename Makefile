CXXFLAGS += -O2 -std=c++23 -Wall -Wextra -Wno-unused-parameter
PLUGIN_FLAGS = -shared -fPIC -fno-gnu-unique $(shell pkg-config --cflags hyprland lua-5.5)

all: hyprmist.so

hyprmist.so: main.cpp Particles.hpp CloudRenderer.hpp Palette.hpp VolumeShader.inc
	$(CXX) $(CXXFLAGS) $(PLUGIN_FLAGS) main.cpp -o $@

VolumeShader.inc: shaders/volume.glsl
	{ printf 'R"GLSL(\n'; cat $<; printf '\n)GLSL"\n'; } > $@

check: tests/particles tests/palette
	./tests/particles
	./tests/palette

tests/palette: tests/palette.cpp Palette.hpp
	$(CXX) $(CXXFLAGS) tests/palette.cpp -o $@

tests/particles: tests/particles.cpp Particles.hpp
	$(CXX) $(CXXFLAGS) tests/particles.cpp -o $@

install: hyprmist.so
	install -Dm755 hyprmist.so $(DESTDIR)$(PREFIX)/lib/hyprland/plugins/hyprmist.so

clean:
	rm -f hyprmist.so tests/particles tests/palette VolumeShader.inc

.PHONY: all check install clean
