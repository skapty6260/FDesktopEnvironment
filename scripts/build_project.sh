export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib

meson setup --wipe build/
meson compile -C build/
cd build
ninja install