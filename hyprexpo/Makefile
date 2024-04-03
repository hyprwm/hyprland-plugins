all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp overview.cpp -o hyprexpo.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b -Wno-narrowing
clean:
	rm ./hyprexpo.so
