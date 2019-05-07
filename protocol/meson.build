wayland_scanner_dep = dependency('wayland-scanner', required: false, native: true)
if wayland_scanner_dep.found()
	wayland_scanner = find_program(
		wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
		native: true,
	)
else
	wayland_scanner = find_program('wayland-scanner', native: true)
endif

protocol_private_src = custom_target(
	'server_private_c',
	input: 'lorie.xml',
	output: 'lorie-protocol-private.c',
	command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
protocol_server_header = custom_target(
	'server_protocol_h',
	input: 'lorie.xml',
	output: 'lorie-server-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
protocol_client_header = custom_target(
	'client_protocol_h',
	input: 'lorie.xml',
	output: 'lorie-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

lorie_server_protocol_static = static_library(
	'lorie_server_static',
	protocol_private_src,
	protocol_server_header,
	pic: true
)

lorie_client_protocol_static = static_library(
	'lorie_client_static',
	protocol_private_src,
	protocol_client_header,
	pic: true
)

liblorie_server_protocol = declare_dependency(
	link_with: lorie_server_protocol_static,
	sources: protocol_server_header
)

liblorie_client_protocol = declare_dependency(
	link_with: lorie_client_protocol_static,
	sources: protocol_client_header
)
