#!/usr/bin/python3

import os
import sys

if __name__ == '__main__':
    srcdir = os.path.dirname(os.path.realpath(__file__))
    top_srcdir = os.path.abspath(os.path.join(srcdir, '..'))
    potfiles = os.path.join(srcdir, 'potfiles.list')
    VERSION = sys.argv[1]
    os.system(f'xgettext --default-domain=elinks \
    --directory={top_srcdir} \
    --msgid-bugs-address=elinks-users@linuxfromscratch.org \
    --add-comments --language=C \
    --keyword=_ --keyword=N_ --keyword=n_:1,2 --keyword=N__ \
    --flag=msg_text:2:c-format --flag=die:1:c-format \
    --flag=secure_fprintf:2:c-format \
    --flag=DBG:1:c-format --flag=elinks_debug:1:c-format \
    --flag=WDBG:1:c-format --flag=elinks_wdebug:1:c-format \
    --flag=ERROR:1:c-format --flag=elinks_error:1:c-format \
    --flag=INTERNAL:1:c-format --flag=elinks_internal:1:c-format \
    --flag=usrerror:1:c-format --flag=elinks_log:4:c-format \
    --flag=LOG_ERR:1:c-format --flag=LOG_WARN:1:c-format \
    --flag=LOG_INFO:1:c-format --flag=LOG_DBG:1:c-format \
    --flag=assertm:2:c-format --flag=elinks_assertm:2:c-format \
    --flag=add_format_to_string:2:c-format \
    --flag=elinks_vsnprintf:3:c-format --flag=elinks_snprintf:3:c-format \
    --flag=elinks_vasprintf:2:c-format --flag=elinks_asprintf:2:c-format \
    --flag=vasprintfa:1:c-format --flag=asprintfa:1:c-format \
    --flag=_:1:pass-c-format --flag=N_:1:pass-c-format \
    --flag=n_:1:pass-c-format --flag=n_:2:pass-c-format \
    --flag=N__:1:pass-c-format \
    -f {potfiles}')
    if not os.path.exists('elinks.po'):
       sys.exit(1)
    os.system(f'perl -I{srcdir}/perl {srcdir}/perl/msgaccel-prepare -S{top_srcdir} elinks.po')
    os.system(f"sed -i -e 's/^\"Project-Id-Version: PACKAGE VERSION/\"Project-Id-Version: ELinks {VERSION}/' elinks.po")
    os.system(f'mv -f elinks.po {srcdir}/elinks.pot')
