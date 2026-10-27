CXXFLAGS += -O2 -std=c++23 -Wall -Wextra -Wno-unused-parameter
PLUGIN_FLAGS = -shared -fPIC -fno-gnu-unique $(shell pkg-config --cflags hyprland lua-5.5)

all: hyprmist.so

hyprmist.so: main.cpp Particles.hpp CloudRenderer.hpp
	$(CXX) $(CXXFLAGS) $(PLUGIN_FLAGS) main.cpp -o $@

check: tests/particles
	./tests/particles

tests/particles: tests/particles.cpp Particles.hpp
	$(CXX) $(CXXFLAGS) tests/particles.cpp -o $@

install: hyprmist.so
	install -Dm755 hyprmist.so $(DESTDIR)$(PREFIX)/lib/hyprland/plugins/hyprmist.so

clean:
	rm -f hyprmist.so tests/particles

.PHONY: all check install clean
