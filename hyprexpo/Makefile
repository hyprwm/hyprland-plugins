# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

all:
	$(CXX) -shared -fPIC $(EXTRA_FLAGS) main.cpp overview.cpp ExpoGesture.cpp OverviewPassElement.cpp -o hyprexpo.so -g `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon` -std=c++2b -Wno-narrowing
clean:
	rm ./hyprexpo.so
