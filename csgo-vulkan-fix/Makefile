all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp -o csgo-vulkan-fix.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./csgo-vulkan-fix.so
