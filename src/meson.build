sources = files(
    'main.c',
    'compositor/compositor.c',
    'util/log.c'
)

wlroots = dependency('wlroots-0.20', version: wlroots_version)
wayland_server = dependency('wayland-server', version: '>=1.21.0')
xkbcommon = dependency('xkbcommon', version: '>=1.5.0')
threads = dependency('threads')

deps = [
    threads,
    wlroots,
    wayland_server,
    xkbcommon
]

executable(
    'fde',
    sources + wl_protos_src,
    include_directories: [fde_inc],
    dependencies: deps,
    install: true 
)