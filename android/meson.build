liblorie_android = shared_module(
	'lorie-android',
	sources: ['lorie_app.c'],
	include_directories: incdirs,
	dependencies: [liblorie_client_protocol],
	link_args: ['-landroid', project_wide_ld_args],
	link_with: lorie_client,
	install: true
)
