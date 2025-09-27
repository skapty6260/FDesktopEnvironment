#!/bin/bash

# build_plugins.sh: Standalone script to compile FDE plugins from ~/.config/fde/plugins/
# Integrates with FDE project root: Links against build libs/includes + system deps.
# Usage: ./build_plugins.sh [clean|--help|--debug]
# Author: Assistant (custom for FDE) - Fixed wlroots detection + arg parsing + separated flags.

set -euo pipefail  # Strict mode: Exit on error, undefined vars, pipe fails.

# Ensure bash (sh may cause shift issues on some systems).
if [[ ! "${BASH_VERSION:-}" ]]; then
    echo "Warning: Use 'bash $0' instead of 'sh $0' for reliable execution." >&2
fi

shopt -s nullglob  # Safely handle non-matching globs.

# Configurable vars (edit for your setup; run from project root recommended).
PLUGINS_DIR="$HOME/.config/fde/plugins"
PROJECT_ROOT="$(pwd)"  # Assume script run from FDE root.
BUILD_DIR="$PROJECT_ROOT/build"  # Meson build dir (as per your change).
GCC="gcc"  # Or "clang"
FDE_INCLUDE_DIR="$PROJECT_ROOT/include"  # Project headers.
DEFAULT_CFLAGS="-std=c11 -fPIC -D_POSIX_C_SOURCE=200809L -DWLR_USE_UNSTABLE -Wall -Wextra"
VERBOSE=true
DEBUG=false
CLEAN=false
HELP=false

# Colors for output.
if command -v tput &> /dev/null; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    YELLOW=$(tput setaf 3)
    RESET=$(tput sgr0)
else
    RED="" GREEN="" YELLOW="" RESET=""
fi

# Helper: Log message with color.
log() {
    local level="$1"
    shift
    local msg="$*"
    case $level in
        INFO) echo "${GREEN}[INFO]${RESET} $msg" ;;
        WARN) echo "${YELLOW}[WARN]${RESET} $msg" ;;
        ERROR) echo "${RED}[ERROR]${RESET} $msg" >&2 ;;
        DEBUG) if [[ $DEBUG == true ]]; then echo "${YELLOW}[DEBUG]${RESET} $msg"; fi ;;
    esac
    if [[ $VERBOSE == false ]]; then return; fi
}

# Helper: Show usage.
show_help() {
    cat << EOF
Usage: $0 [clean|--help|--debug]
  clean: Remove old .so files without rebuilding.
  --help: Show this help.
  --debug: Print all compile/link flags for debugging.
Run from FDE project root for best integration (uses build libs/includes).
Requires: gcc, pkg-config, wlroots, wayland, xkbcommon (pacman -S wlroots wayland xkbcommon).
EOF
    exit 0
}

# FIXED: Standard argument parsing (no early if; handle all in loop, process after).
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            DEBUG=true
            shift
            ;;
        clean)
            CLEAN=true
            shift
            ;;
        --help)
            HELP=true
            shift
            ;;
        *)
            log ERROR "Unknown argument: $1"
            show_help
            exit 1
            ;;
    esac
done

# Process flags after loop (no shift issues).
if [[ $HELP == true ]]; then
    show_help
fi

if [[ $CLEAN == true ]]; then
    mkdir -p "$PLUGINS_DIR"
    log INFO "Cleaning old .so files in $PLUGINS_DIR"
    find "$PLUGINS_DIR" -maxdepth 1 -name "*.so" -delete
    exit 0
fi

# Helper: Get pkg-config flags (separated into cflags and libs; Arch-friendly).
get_pkg_flags() {
    local pkgs_arch="$1"  # e.g., "wlroots wayland-server xkbcommon"
    local pkgs_deb="$2"   # Fallback: "wlroots-0.20 wayland-server xkbcommon"

    if command -v pkg-config &> /dev/null; then
        # Try Arch-style first.
        if SYSTEM_CFLAGS=$(pkg-config --cflags "$pkgs_arch" 2>/dev/null); then
            SYSTEM_LIBS=$(pkg-config --libs "$pkgs_arch" 2>/dev/null)
        # Fallback to Debian/Ubuntu-style.
        elif SYSTEM_CFLAGS=$(pkg-config --cflags "$pkgs_deb" 2>/dev/null); then
            SYSTEM_LIBS=$(pkg-config --libs "$pkgs_deb" 2>/dev/null)
        fi
    else
        log WARN "pkg-config not found; using fallback paths."
        SYSTEM_CFLAGS=""
        SYSTEM_LIBS=""
    fi
}

# Helper: Detect wlroots include dir (searches for actual log.h file).
detect_wlroots_inc() {
    local possible_dirs=(
        "/usr/local/include/wlroots-0.20"
        "/usr/include/wlroots-0.20"
        "/usr/local/include/wlroots"
        "/usr/include/wlroots"
        "/usr/local/include/wlroots-*"  # Glob for versions.
    )
    for dir in "${possible_dirs[@]}"; do
        if [[ $dir == * "*" ]]; then
            dir=$(echo $dir | tr '*' '*')  # Handle glob.
            for d in $dir; do
                if [[ -f "$d/wlr/util/log.h" ]]; then
                    echo "-I$d"
                    return 0
                fi
            done
        elif [[ -f "$dir/wlr/util/log.h" ]]; then
            echo "-I$dir"
            return 0
        fi
    done
    log ERROR "No wlroots headers found (searched common paths). Install wlroots dev package."
    return 1
}

# Helper: Get project-specific flags (from build; mimics Meson linkage).
get_project_flags() {
    local project_cflags="-I$FDE_INCLUDE_DIR -I$BUILD_DIR"  # Project + generated includes.
    local project_libs="-L$BUILD_DIR/utils -L$BUILD_DIR -lfdeutils -ldl"  # Project libs + dl for dlopen.

    if [[ -f "$BUILD_DIR/meson-logs/meson-log.txt" ]]; then
        log DEBUG "Using Meson build for project linkage."
    fi

    echo "$project_cflags $project_libs"
}

# Helper: Parse build.txt into vars (key=value, ignore # comments).
parse_build_txt() {
    local build_txt="$1"
    local -A config=()

    if [[ ! -f "$build_txt" ]]; then
        log WARN "No build.txt in $PWD; using defaults."
        return
    fi

    while IFS='=' read -r key value; do
        key="${key%%[[:space:]]*}"; key="${key#"${key%%[![:space:]]*}"}"  # Trim spaces.
        value="${value#"${value%%[![:space:]]*}"}"; value="${value%"${value##*[![:space:]]}"}"  # Trim spaces.
        [[ $key == \#* || -z "$key" ]] && continue
        config["$key"]="$value"
    done < "$build_txt"

    for key in "${!config[@]}"; do
        export "${key^^}"="${config[$key]}"
    done
}

# Helper: Get default sources (all .c in dir).
get_default_sources() {
    local sources=$(find . -maxdepth 1 -name "*.c" -printf "%f " | tr -d '\n')
    if [[ -z "$sources" ]]; then
        log ERROR "No .c files found in $PWD"
        return 1
    fi
    echo "$sources"
}

# Helper: Compile single .c to .o (with debug for full flags).
compile_object() {
    local src="$1"
    local obj="${src%.c}.o"
    local all_cflags="$DEFAULT_CFLAGS $CFLAGS $INCLUDES"

    if [[ $DEBUG == true ]]; then
        log DEBUG "Full CFLAGS for $src: $all_cflags"
    fi

    log INFO "Compiling $src -> $obj"
    if ! $GCC $all_cflags -c "$src" -o "$obj"; then
        log ERROR "Failed to compile $src"
        return 1
    fi
}

# Helper: Link .o files to .so.
link_shared() {
    local output="$1"
    local all_libs="$DEFAULT_LIBS $LIBS"  # Now pure libs (no mixed -I).
    local objs=$(find . -maxdepth 1 -name "*.o" -printf "%f " | tr -d '\n')

    if [[ -z "$objs" ]]; then
        log ERROR "No .o files to link in $PWD"
        return 1
    fi

    if [[ $DEBUG == true ]]; then
        log DEBUG "Full LDFLAGS for $output: $all_libs"
    fi

    log INFO "Linking $objs -> $output"
    if ! $GCC -shared $objs $all_libs -o "$output"; then
        log ERROR "Failed to link $output"
        return 1
    fi

    rm -f *.o
    log INFO "Built $output successfully"
}

# Validate project root/build.
if [[ ! -d "$FDE_INCLUDE_DIR" ]]; then
    log ERROR "FDE include dir $FDE_INCLUDE_DIR not found. Run from project root?"
    exit 1
fi
if [[ ! -d "$BUILD_DIR" ]]; then
    log ERROR "Build dir $BUILD_DIR not found. Run 'meson setup build && meson compile -C build' first."
    exit 1
fi
if [[ ! -f "$BUILD_DIR/utils/libfdeutils.so" ]]; then
    log WARN "libfdeutils.so not in $BUILD_DIR/utils. Build: meson compile -C build utils"
fi

# Get dynamic system flags (separated).
get_pkg_flags "wlroots wayland-server xkbcommon" "wlroots-0.20 wayland-server xkbcommon"

# Fallback if pkg-config failed (use detection for wlroots inc).
if [[ -z "$SYSTEM_CFLAGS" ]]; then
    log WARN "pkg-config failed; using fallback detection."
    wlroots_inc=$(detect_wlroots_inc)
    if [[ $? -ne 0 ]]; then
        exit 1
    fi
    SYSTEM_CFLAGS="-I/usr/include $wlroots_inc"  # Basic + detected wlroots.
    SYSTEM_LIBS="-lwlroots-0.20 -lwayland-server -lxkbcommon -lm"  # Fallback libs (adjust version if needed).
fi

# Get project flags.
PROJECT_FLAGS=$(get_project_flags)
PROJECT_CFLAGS=$(echo "$PROJECT_FLAGS" | cut -d' ' -f1- )  # Includes only (before -L).
PROJECT_LIBS=$(echo "$PROJECT_FLAGS" | cut -s -d' ' -f- )  # Libs (after -L, but simple split).

# Set defaults (clean separation).
SYSTEM_INCLUDES="-I/usr/include $SYSTEM_CFLAGS"  # Pure includes.
DEFAULT_LIBS="$SYSTEM_LIBS $PROJECT_LIBS"  # Pure libs.
FDE_LIB_DIR="$BUILD_DIR/utils"  # For any extra -L if needed.

# Ensure dir exists.
mkdir -p "$PLUGINS_DIR"
log INFO "Scanning plugins in $PLUGINS_DIR (project root: $PROJECT_ROOT)"

# Check gcc and pkg-config.
if ! command -v "$GCC" &> /dev/null; then
    log ERROR "Compiler $GCC not found. Install: pacman -S gcc"
    exit 1
fi
if ! command -v pkg-config &> /dev/null; then
    log WARN "pkg-config not found; fallback used."
fi

# Debug: Print flags if --debug.
if [[ $DEBUG == true ]]; then
    log INFO "Debug: SYSTEM_INCLUDES = $SYSTEM_INCLUDES"
    log INFO "Debug: DEFAULT_LIBS = $DEFAULT_LIBS"
    log INFO "Debug: DEFAULT_CFLAGS = $DEFAULT_CFLAGS"
fi

# Scan subdirs.
success_count=0
fail_count=0
cd "$PLUGINS_DIR" || { log ERROR "Cannot access $PLUGINS_DIR"; exit 1; }

for plugin_dir in */; do
    plugin_dir="${plugin_dir%/}"
    if [[ ! -d "$plugin_dir" ]]; then continue; fi

    if ! ls "$plugin_dir"/*.c >/dev/null 2>&1 && [[ ! -f "$plugin_dir/build.txt" ]]; then
        log WARN "Skipping $plugin_dir (no .c files or build.txt)"
        continue
    fi

    log INFO "--- Building plugin: $plugin_dir ---"
    pushd "$plugin_dir" > /dev/null || { log ERROR "Cannot enter $plugin_dir"; ((fail_count++)); continue; }

    parse_build_txt "build.txt"

    : "${NAME:="$plugin_dir"}"
    : "${SOURCES:="$(get_default_sources)"}"
    if [[ $? -ne 0 ]]; then
        ((fail_count++))
        popd > /dev/null
        continue
    fi
    : "${INCLUDES:="$SYSTEM_INCLUDES -I$FDE_INCLUDE_DIR"}"
    : "${LIBS:="$DEFAULT_LIBS"}"
    : "${CFLAGS:=""}"

    if [[ $DEBUG == true ]]; then
        log INFO "Debug for $plugin_dir: INCLUDES=$INCLUDES LIBS=$LIBS CFLAGS=$DEFAULT_CFLAGS $CFLAGS"
    fi

    # Compile each source to .o.
    success=true
    for src in $SOURCES; do
        if [[ ! -f "$src" ]]; then
            log ERROR "Source $src not found in $plugin_dir"
            success=false
            break
        fi
        compile_object "$src" || { success=false; break; }
    done

    if [[ $success == false ]]; then
        ((fail