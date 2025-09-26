4. Сборка плагинов с Meson и скриптом
Каждый плагин — mini-проект с meson.build. Скрипт scripts/build_plugins.sh автоматизирует: build → copy to ~/.config/fde/plugins/ → trigger hot-reload (touch или signal).

## How to build plugins
cd fde
sh ./scripts/build_plugins.sh          # Build all plugins.
sh ./scripts/build_plugins.sh "plugin name"  # Specific.

# После: .so в ~/.config/fde/plugins/. Запустите compositor — auto-load.
# Измените код плагина, re-run script — hot-reload сработает (touch triggers inotify).

my-compositor/  # Root проекта (git repo).
├── meson.build  # Для core (compositor executable).
├── src/         # Core sources.
│   ├── main.c
│   ├── compositor_core.c
│   ├── plugin_system.c
│   ├── config.c
│   └── ... (cli.c etc.)
├── include/     # Headers.
│   ├── compositor_core.h
│   ├── plugin_system.h
│   └── compositor_api.h
├── plugins/     # Исходники примеров плагинов (каждый — subdir с meson.build).
│   ├── custom_render/  # Пример: custom_render.c, meson.build
│   ├── settings_client/ # settings_client.c, meson.build
│   └── ...
├── scripts/     # Скрипты.
│   └── build_plugins.sh  # Автоматизация сборки.
└── README.md    # Docs.

Runtime dirs
~/.config/fde/
├── config.ini          # Конфиг (INI).
└── plugins/            # .so файлы (hot-reload watches here).
    ├── custom_render.so
    └── settings_client.so