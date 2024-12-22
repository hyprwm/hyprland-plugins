all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp trail.cpp TrailPassElement.cpp -o hyprtrails.so -g `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon` -std=c++2b -O2
clean:
	rm ./hyprtrails.so
