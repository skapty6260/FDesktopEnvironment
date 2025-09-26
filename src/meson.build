sources = files(
    'main.c',
    'config.c',
    'plugin-system.c',
    'utils/log.c'   
)

wlroots = dependency('wlroots-0.20')
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