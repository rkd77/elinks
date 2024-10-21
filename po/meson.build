if conf_data.get('CONFIG_NLS')
    i18n = import('i18n')
    # define GETTEXT_PACKAGE
    add_project_arguments('-DGETTEXT_PACKAGE="elinks"', language:'c')
    i18n.gettext('elinks',
        args: '--directory=' + meson.project_source_root()
    )
endif

generate_potfiles = find_program('potfiles.py')
potfiles = custom_target('generate-potfiles', command:[generate_potfiles], output:'potfiles.list')

generate_pot = find_program('generate_pot.py')
generate_pot_tgt = custom_target('generate-pot', command:[generate_pot, meson.project_version()], depends:[potfiles], output:'elinks.pot')

check_po_sh = find_program('check_po.sh')
run_target('check-po', command:[check_po_sh])

update_po_sh = find_program('update_po.sh')
run_target('update-po', command:[update_po_sh], depends:[generate_pot_tgt])
