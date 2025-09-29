wlroots = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server', version: '>=1.21.0')
xkbcommon = dependency('xkbcommon', version: '>=1.5.0')
threads = dependency('threads')
dbus = dependency('dbus-1')
glib = dependency('glib-2.0')

deps = [
    threads,
    wlroots,
    wayland_server,
    xkbcommon,
    dbus
]

# Generate client_code
gdbus_codegen = find_program('gdbus-codegen', required: true)

r = run_command(gdbus_codegen, '--generate-c-code=/usr/local/share/org/fde/org.fde.Compositor', '--c-namespace=OrgFdeCompositor', 'dbus/org.fde.Compositor.xml')
if r.returncode() != 0
    error('Cannot generate gdbus .h and .c code from dbus .xml')
endif

sources = files(
    'main.c',
    'config.c',
    'compositor.c',
    'plugin-system.c',
    'dbus/dbus.c',
    'dbus/config.c',
    'dbus/core.c',
    'dbus/plugins.c',
    'utils/log.c',
)

orgFdeCompositor_inc = include_directories('/usr/local/share/org/fde/')

executable(
    'fde',
    sources + wl_protos_src,
    include_directories: [fde_inc, orgFdeCompositor_inc],
    dependencies: deps,
    install: true 
)

test_plugin = executable('test-plugin',
                         files(
                            'test-plugin.c',
                            '/usr/local/share/org/fde/org.fde.Compositor.c'
                         ),
                         include_directories: [include_directories('..'), orgFdeCompositor_inc],  # Для fde/utils, если нужно (опционально)
                         dependencies: [
                            dbus,
                            dependency('gio-2.0'),
                            dependency('glib-2.0')
                         ],
                         install: true,  # Не устанавливать; для теста
                         install_dir: '/home/dietcokelover/.config/fde/plugins',  # Опционально: путь установки
                         c_args: ['-g', '-O0', '-Wall'])  # Отладка, как в gcc