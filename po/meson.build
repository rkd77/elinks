if conf_data.get('CONFIG_NLS')
    i18n = import('i18n')
    # define GETTEXT_PACKAGE
    add_project_arguments('-DGETTEXT_PACKAGE="elinks"', language:'c')
    i18n.gettext('elinks',
        args: '--directory=' + meson.source_root()
    )
endif

check_po_sh = find_program('check_po.sh')
run_target('check-po', command:[check_po_sh])
