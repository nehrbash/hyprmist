QSB ?= qsb
all: volume.frag.qsb composite.frag.qsb
%.frag.qsb: %.frag ../shaders/volume.glsl
	$(QSB) --glsl '100 es,120,150' -o $@ $<
clean:
	rm -f *.qsb
.PHONY: all clean
