CXXFLAGS = -shared -fPIC -Wl,--no-gnu-unique -g -std=c++2b
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland pangocairo`
LIBS = `pkg-config --libs pangocairo`

SRC = main.cpp barDeco.cpp
TARGET = hyprbars.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $> -o $@ $(LIBS)

clean:
	rm ./$(TARGET)

meson-build:
	mkdir -p build
	cd build && meson .. && ninja

.PHONY: all meson-build clean
