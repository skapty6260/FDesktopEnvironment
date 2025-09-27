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

# DBUS (Plugins and IPC)
dbus_xml = files('dbus/org.fde.Compositor.xml')

# Поиск gdbus-codegen (опционально)
gdbus_codegen = find_program('gdbus-codegen', required: false)

# Генерация C-кода (только если gdbus-codegen найден)
generated_inc = include_directories('.')  # Для generated .h

if gdbus_codegen.found() and glib.found()
    deps += [glib]  # Добавляем glib только если используется
    dbus_generated_c = custom_target(
        'dbus-generated.c',
        input: dbus_xml,
        output: 'org.fde.Compositor-generated.c',
        command: [
            gdbus_codegen,
            '--interface-prefix', 'org.fde.',
            '--c-namespace', 'Fde',
            '--generate-c-code', '@BASENAME@',
            '@INPUT@'
        ]
    )

    add_project_arguments('-DHAVE_GDBUS=1', language: 'c')
    message('D-Bus code generation enabled with GDBus (using generated stubs)')
else
    dbus_generated_c = null  # Не используется, но для ясности
    add_project_arguments('-DHAVE_GDBUS=0', language: 'c')
    message('gdbus-codegen not found: Using raw libdbus-1 (manual D-Bus code)')
endif

# Установка XML для introspection (всегда)
install_data(
    dbus_xml,
    install_dir: get_option('datadir') / 'dbus-1' / 'interfaces'
)

sources = files(
    'main.c',
    'config.c',
    'compositor.c',
    'dbus/plugin-system.c',
    'dbus/dbus.c',
    'utils/log.c'
)

executable(
    'fde',
    sources + wl_protos_src,
    include_directories: [fde_inc, generated_inc],
    dependencies: deps,
    install: true 
)