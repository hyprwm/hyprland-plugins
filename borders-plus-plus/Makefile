all:
	g++ -shared -fPIC --no-gnu-unique main.cpp borderDeco.cpp -o borders-plus-plus.so -g $(shell pkg-config --cflags pixman-1 libdrm hyprland) -std=c++23
clean:
	rm ./borders-plus-plus.so
