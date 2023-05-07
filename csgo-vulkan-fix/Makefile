all:
	g++ -shared -fPIC --no-gnu-unique main.cpp -o csgo-vulkan-fix.so -g $(shell pkg-config --cflags pixman-1 libdrm hyprland) -std=c++23
clean:
	rm ./csgo-vulkan-fix.so
