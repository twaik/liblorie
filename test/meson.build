test_server = executable(
	'test-server',
	sources: ['test-server.c', protocol_server_header],
	include_directories: incdirs,
	link_with: lorie_server
)

libx11 = dependency('x11')

test_renderer = executable(
	'test-renderer',
	sources: ['X11.c', '../src/renderer/renderer.c', protocol_client_header],
	include_directories: incdirs,
	link_with: lorie_client,
	dependencies: [egl, glesv2, libx11, liblorie_client_protocol],
	link_args: ['-lm']
)

test_client = executable(
	'test-client',
	sources: ['test-client.c', 'x11-window.c', protocol_client_header],
	include_directories: incdirs,
	link_with: lorie_client,
	dependencies: [egl, glesv2, libx11]
)
