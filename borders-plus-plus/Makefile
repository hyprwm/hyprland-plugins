all:
	$(CXX) -shared -fPIC -Wl,--no-gnu-unique main.cpp borderDeco.cpp -o borders-plus-plus.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./borders-plus-plus.so
