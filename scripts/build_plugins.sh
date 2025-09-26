#!/bin/bash
# build_plugins.sh: Build all plugins and install to ~/.config/fde/plugins/
# Usage: ./scripts/build_plugins.sh [plugin_name]  # All or specific.

set -e  # Exit on error.

# Root dir (script location).
ROOT_DIR="$(dirname "$(realpath "$-1")")"
PLUGINS_SRC="$ROOT_DIR/plugins"
XDG_CONFIG="$HOME/.config/fde/plugins"

# Ensure target dir.
mkdir -p "$XDG_CONFIG"

# Function to build one plugin.
build_plugin() {
    local plugin_name="$1"
    local src_dir="$PLUGINS_SRC/$plugin_name"
    if [ ! -d "$src_dir" ]; then
        echo "Plugin dir not found: $src_dir"
        return 1
    fi

    echo "Building $plugin_name..."
    cd "$src_dir"

    # Meson setup if not exists.
    if [ ! -d "build" ]; then
        meson setup build/
    fi
    meson compile -C build/

    # Copy .so to XDG (override if exists).
    local so_file="build/lib${plugin_name}.so"  # Meson naming: libname.so.
    if [ -f "$so_file" ]; then
        cp "$so_file" "$XDG_CONFIG/${plugin_name}.so"
        echo "Installed $plugin_name.so to $XDG_CONFIG"

        # Trigger hot-reload: Touch to notify inotify (or kill -USR1 compositor_pid).
        touch "$XDG_CONFIG/${plugin_name}.so"
        echo "Touched $plugin_name.so for hot-reload"
    else
        echo "Build failed: $so_file not found"
    fi

    cd "$ROOT_DIR"
}

# Build all or specific.
if [ $# -eq 0 ]; then
    for plugin in "$PLUGINS_SRC"/*/; do
        plugin_name=$(basename "$plugin")
        build_plugin "$plugin_name"
    done
else
    for arg in "$@"; do
        build_plugin "$arg"
    done
fi

echo "Build complete. Run compositor for hot-reload."