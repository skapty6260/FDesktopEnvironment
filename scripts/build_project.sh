export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib

rm -rf build/  # clean build dir to avoid permission issues
meson setup build/
meson compile -C build/
cd build
ninja install