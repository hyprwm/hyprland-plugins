all:
	g++ -shared -fPIC --no-gnu-unique main.cpp -o csgo-vulkan-fix.so -g -I "/usr/include/pixman-1" -I "/usr/include/libdrm" $(shell pkg-config --cflags hyprland) -std=c++23
clean:
	rm ./csgo-vulkan-fix.so
