if conf_data.get('CONFIG_NLS') == 1
    i18n = import('i18n')
    # define GETTEXT_PACKAGE
    add_project_arguments('-DGETTEXT_PACKAGE="elinks"', language:'c')
    i18n.gettext('elinks',
        args: '--directory=' + meson.source_root()
    )
endif
