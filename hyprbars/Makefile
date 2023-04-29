CXX = g++
CXXFLAGS = -shared -fPIC --no-gnu-unique -g -std=c++23
INCLUDES = -I "/usr/include/pixman-1" -I "/usr/include/libdrm" -I $(shell pkg-config --cflags hyprland pangocairo)
LIBS = $(shell pkg-config --libs pangocairo)

SRC = main.cpp barDeco.cpp
TARGET = hyprbars.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

clean:
	rm ./$(TARGET)
