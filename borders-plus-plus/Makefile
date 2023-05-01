all:
	g++ -shared -fPIC --no-gnu-unique main.cpp borderDeco.cpp -o borders-plus-plus.so -g -I "/usr/include/pixman-1" -I "/usr/include/libdrm" $(shell pkg-config --cflags hyprland) -std=c++23
clean:
	rm ./borders-plus-plus.so
