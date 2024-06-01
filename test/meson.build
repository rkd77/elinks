if get_option('test')
	h = run_command(meson.source_root()+'/test/ecmascript/assert/assert.sh')
	tests = h.stdout().strip().split('\n')
	foreach t : tests
		name = t.split('/')[-1]
		test(name, elinks, protocol: 'exitcode', is_parallel: false, workdir: meson.current_build_dir(),
			args: ['--test', '1', '--eval', 'set ecmascript.enable = 1',
			'--eval', 'set ecmascript.enable_console_log = 1',
			'--eval', 'set ui.sessions.fork_on_start = 0',
		t])
	endforeach
endif
