xorg_server = dependency('xorg-server', version: '>=1.20.1')
xorg_link_args = []
if get_option('android_client')
	xorg_link_args += '-lXFree86'
endif

xf86_mixed_lorie = shared_module(
	'lorie_drv',
	name_prefix: '',
	sources: ['dummy_cursor.c', 'dummy_driver.c', 'dummy_input.c'],
	include_directories: incdirs,
	c_args: [
		'-DHAVE_CONFIG_H', 
		''.join(['-I', xorg_server.get_pkgconfig_variable('sdkdir')])
		],
	dependencies: [xorg_server, liblorie_server_protocol],
	link_args: xorg_link_args,
	link_with: lorie_server,
	install_dir: join_paths([xorg_server.get_pkgconfig_variable('moduledir'), 'drivers']),
	install: true
)

meson.add_install_script('post_install.sh', xorg_server.get_pkgconfig_variable('moduledir'))
