project(
	'liblorie',
	'c',
	version: '0.1.1',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c11',
		'warning_level=1',
		'b_lundef=false',
		'prefix=/usr'
	],
)

pkg = import('pkgconfig')
compiler = meson.get_compiler('c')
wayland_server = dependency('wayland-server', version: '>=1.16')
wayland_client = dependency('wayland-client')
egl = compiler.find_library('EGL')
glesv2 = compiler.find_library('GLESv2')

incdirs = include_directories('src', 'src/renderer')
project_wide_ld_args = []
if get_option('android_client')
	project_wide_ld_args += '-llog'
endif

subdir('protocol')
subdir('src')

if get_option('build_tests')
	subdir('test')
endif

if get_option('xorg_module')
	subdir('xf86-mixed-lorie')
endif

if get_option('android_client')
	subdir('android')
endif
