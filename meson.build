project(
    'fde',
    'c',
    version: '1.01-dev',
    license: 'MIT',
    meson_version: '>=1.3',
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=true',
        'wrap_mode=nodownload',
    ],
)

add_project_arguments(
    [
		'-DWLR_USE_UNSTABLE',
		'-D_POSIX_C_SOURCE=200809L',

		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-unused-but-set-variable',
		'-Wno-missing-braces',
		'-Wno-format-zero-length',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')

if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

# wlroots sub
wlroots_version = ['>=0.20.0', '<0.21.0']
subproject(
	'wlroots',
	default_options: ['examples=false'],
	required: false,
	version: wlroots_version,
)
wlroots = dependency('wlroots-0.20') # fallback: 'wlroots' (TODO)
wlroots_features = {
	'xwayland': false,
	'libinput_backend': false,
	'session': false,
}
foreach name, _ : wlroots_features
	var_name = 'have_' + name.underscorify()
	have = wlroots.get_variable(pkgconfig: var_name, internal: var_name) == 'true'
	wlroots_features += { name: have }
endforeach

null_dep = dependency('', required: false)

wayland_server = dependency('wayland-server', version: '>=1.21.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])
xkbcommon = dependency('xkbcommon', version: '>=1.5.0')
threads = dependency('threads') # for pthread_setschedparam and pthread_atfork
m_dep = meson.get_compiler('c').find_library('m', required: true)
dl_dep = meson.get_compiler('c').find_library('dl', required: true)

conf_data = configuration_data()

version = '"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
	git_commit = run_command([git, '--git-dir=.git', 'rev-parse', '--short', 'HEAD'], check: false)
	git_branch = run_command([git, '--git-dir=.git', 'rev-parse', '--abbrev-ref', 'HEAD'], check: false)
	if git_commit.returncode() == 0 and git_branch.returncode() == 0
		version = '"@0@-@1@ (" __DATE__ ", branch \'@2@\')"'.format(
			meson.project_version(),
			git_commit.stdout().strip(),
			git_branch.stdout().strip(),
		)
	endif
endif
add_project_arguments('-DFDE_VERSION=@0@'.format(version), language: 'c')

fs = import('fs')

# Strip relative path prefixes from the code if possible, otherwise hide them.
relative_dir = fs.relative_to(meson.current_source_dir(), meson.global_build_root()) + '/'
if cc.has_argument('-fmacro-prefix-map=/prefix/to/hide=')
	add_project_arguments(
		'-fmacro-prefix-map=@0@='.format(relative_dir),
		language: 'c',
	)
else
	add_project_arguments(
		'-DSWAY_REL_SRC_DIR="@0@"'.format(relative_dir),
		language: 'c',
	)
endif

fde_inc = include_directories('include')

subdir('include')
subdir('protocols')
subdir('src')

config = configuration_data()
config.set('datadir', join_paths(prefix, datadir))
config.set('prefix', prefix)
config.set('sysconfdir', join_paths(prefix, sysconfdir))

configure_file(
	configuration: config,
	input: 'config.ini',
	output: '@BASENAME@',
	install_dir: join_paths(sysconfdir, 'fde')
)