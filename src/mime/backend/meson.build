if conf_data.get('CONFIG_DGI')
	srcs += files('dgi.c')
endif
if conf_data.get('CONFIG_MAILCAP')
	srcs += files('mailcap.c')
endif
if conf_data.get('CONFIG_MIMETYPES')
	srcs += files('mimetypes.c')
endif

srcs += files('common.c', 'default.c')

if conf_data.get('CONFIG_MAILCAP') and get_option('test')
	mailcap_cache_files = files('common.c', 'mailcap.c', meson.source_root()+'/src/osdep/osdep.c')

	if conf_data.get('CONFIG_NLS') and not conf_data.get('CONFIG_GETTEXT')
		mailcap_cache_files += files(
		meson.source_root()+'/src/intl/gettext/bindtextdom.c',
		meson.source_root()+'/src/intl/gettext/dcgettext.c',
		meson.source_root()+'/src/intl/gettext/dcigettext.c',
		meson.source_root()+'/src/intl/gettext/dcngettext.c',
		meson.source_root()+'/src/intl/gettext/dgettext.c',
		meson.source_root()+'/src/intl/gettext/dngettext.c',
		meson.source_root()+'/src/intl/gettext/explodename.c',
		meson.source_root()+'/src/intl/gettext/finddomain.c',
		meson.source_root()+'/src/intl/gettext/gettext.c',
		meson.source_root()+'/src/intl/gettext/intl-compat.c',
		meson.source_root()+'/src/intl/gettext/l10nflist.c',
		meson.source_root()+'/src/intl/gettext/libintl.c',
		meson.source_root()+'/src/intl/gettext/loadmsgcat.c',
		meson.source_root()+'/src/intl/gettext/localcharset.c',
		meson.source_root()+'/src/intl/gettext/localealias.c',
		meson.source_root()+'/src/intl/gettext/ngettext.c',
		meson.source_root()+'/src/intl/gettext/plural.c',
		meson.source_root()+'/src/intl/gettext/textdomain.c')
	endif

	if conf_data.get('CONFIG_NLS') and conf_data.get('CONFIG_GETTEXT')
		mailcap_cache_files += files(
		meson.source_root()+'/src/intl/libintl.c',
		meson.source_root()+'/src/util/env.c')
	endif

	exe = executable('mailcap-cache', mailcap_cache_files, testdeps,
	c_args:['-DHAVE_CONFIG_H', '-DTEST_MAILCAP'], include_directories:['.','..','../..','../../..'],
	build_by_default:false, dependencies:[x11deps])
	test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
	t = find_program('test-mailcap-cache')
	test('mailcap-cache', t, depends:[exe], env:test_lib, workdir:meson.current_build_dir())
endif
