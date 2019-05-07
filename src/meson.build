lorie_server = shared_module(
	'lorie-server',
	sources: ['server.c', 'buffer.c'],
	include_directories: incdirs,
	dependencies: [wayland_server, liblorie_server_protocol],
	link_args: [project_wide_ld_args],
	install: true
)

lorie_client = shared_module(
	'lorie-client',
	sources: ['client.c', 'buffer.c', 'renderer/renderer.c'],
	include_directories: incdirs,
	dependencies: [wayland_client, egl, glesv2, liblorie_client_protocol],
	link_args: [project_wide_ld_args],
	install: true
)
