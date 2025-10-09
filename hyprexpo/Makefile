# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS = -shared -fPIC -g -std=c++2b -Wno-c++11-narrowing -Wno-narrowing
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon`
LIBS = `pkg-config --libs pangocairo`

SRC = main.cpp overview.cpp ExpoGesture.cpp OverviewPassElement.cpp scrollOverview.cpp
TARGET = hyprexpo.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

clean:
	rm -f ./$(TARGET)

.PHONY: all clean
