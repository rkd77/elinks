project('elinks', ['c','cpp'], version:'0.19.GIT', license:'GPLv2', default_options:['cpp_std=gnu++17'], meson_version:'>=1.0.1')
srcs=[]

srcdir = meson.project_source_root()

git = find_program('git', required: false)

if git.found()
    txt = run_command('git', '--git-dir=' + srcdir + '/.git', 'rev-parse', 'HEAD', check:false).stdout().strip()
    dirty = run_command(srcdir / 'git-dirty.sh', check:false).stdout().strip()
else
    txt = ''
    dirty = ''
endif
add_global_arguments('-DBUILD_ID="' + txt + dirty + '"', language : 'c')

conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
conf_data.set('PACKAGE', meson.project_name())
conf_data.set('DATE', run_command('date', '%Y-%m-%d', check:false).stdout().strip())

conf_data.set('CONFIG_LARGEFILE', get_option('largefile'))
conf_data.set('CONFIG_BOOKMARKS', get_option('bookmarks'))
conf_data.set('CONFIG_XBEL_BOOKMARKS', get_option('xbel'))
conf_data.set('CONFIG_SCRIPTING_SPIDERMONKEY', get_option('sm-scripting'))
conf_data.set('CONFIG_NLS', get_option('nls'))
conf_data.set('CONFIG_GETTEXT', get_option('gettext'))

conf_data.set('CONFIG_COOKIES', get_option('cookies'))
conf_data.set('CONFIG_FORMHIST', get_option('formhist'))
conf_data.set('CONFIG_GLOBHIST', get_option('globhist'))
conf_data.set('CONFIG_MAILCAP', get_option('mailcap'))
conf_data.set('CONFIG_MIMETYPES', get_option('mimetypes'))
conf_data.set('CONFIG_IPV6', get_option('ipv6'))
conf_data.set('CONFIG_BITTORRENT', get_option('bittorrent'))
conf_data.set('CONFIG_DATA', get_option('data'))
conf_data.set('CONFIG_GEMINI', get_option('gemini'))
conf_data.set('CONFIG_SPARTAN', get_option('spartan'))
conf_data.set('CONFIG_KITTY', get_option('kitty'))

conf_data.set('CONFIG_URI_REWRITE', get_option('uri-rewrite'))
conf_data.set('CONFIG_CGI', get_option('cgi'))
conf_data.set('CONFIG_DGI', get_option('dgi'))
conf_data.set('CONFIG_FINGER', get_option('finger'))
conf_data.set('CONFIG_FSP', get_option('fsp'))
conf_data.set('CONFIG_FSP2', get_option('fsp2'))
conf_data.set('CONFIG_FTP', get_option('ftp'))
conf_data.set('CONFIG_SFTP', get_option('sftp'))
conf_data.set('CONFIG_GOPHER', get_option('gopher'))
conf_data.set('CONFIG_NNTP', get_option('nntp'))
conf_data.set('CONFIG_SMB', get_option('smb'))
conf_data.set('CONFIG_MOUSE', get_option('mouse'))
conf_data.set('CONFIG_SYSMOUSE', get_option('sysmouse'))
conf_data.set('CONFIG_88_COLORS', get_option('88-colors'))

conf_data.set('CONFIG_256_COLORS', get_option('256-colors'))
conf_data.set('CONFIG_TRUE_COLOR', get_option('true-color'))
conf_data.set('CONFIG_EXMODE', get_option('exmode'))
conf_data.set('CONFIG_LEDS', get_option('leds'))
conf_data.set('CONFIG_MARKS', get_option('marks'))
conf_data.set('CONFIG_CSS', get_option('css'))
conf_data.set('CONFIG_DOM', get_option('html-highlight'))

conf_data.set('CONFIG_BACKTRACE', get_option('backtrace'))
conf_data.set('CONFIG_NO_ROOT_EXEC', get_option('no-root'))
conf_data.set('CONFIG_DEBUG', get_option('withdebug'))
conf_data.set('CONFIG_MEMCOUNT', get_option('memcount'))
conf_data.set('CONFIG_FASTMEM', get_option('fastmem'))
conf_data.set('CONFIG_OWN_LIBC', get_option('own-libc'))
conf_data.set('CONFIG_SMALL', get_option('small'))
conf_data.set('CONFIG_UTF8', get_option('utf-8'))

conf_data.set('CONFIG_XTERM', get_option('xterm'))
conf_data.set('CONFIG_GPM', get_option('gpm'))
conf_data.set('CONFIG_TERMINFO', get_option('terminfo'))
conf_data.set('CONFIG_GZIP', get_option('zlib'))
conf_data.set('CONFIG_BZIP2', get_option('bzlib'))
conf_data.set('CONFIG_BROTLI', get_option('brotli'))

conf_data.set('CONFIG_ZSTD', get_option('zstd'))
conf_data.set('CONFIG_IDN2', get_option('idn'))
conf_data.set('CONFIG_LZMA', get_option('lzma'))
conf_data.set('CONFIG_GSSAPI', get_option('gssapi'))
conf_data.set('CONFIG_ECMASCRIPT_SMJS', get_option('spidermonkey'))
conf_data.set('CONFIG_SCRIPTING_GUILE', get_option('guile'))
conf_data.set('CONFIG_SCRIPTING_PERL', get_option('perl'))
conf_data.set('CONFIG_SCRIPTING_PYTHON', get_option('python'))
conf_data.set('CONFIG_LIBSIXEL', get_option('libsixel'))
conf_data.set('CONFIG_LIBWEBP', get_option('libwebp'))
conf_data.set('CONFIG_LIBAVIF', get_option('libavif'))

conf_data.set('CONFIG_REPRODUCIBLE', get_option('reproducible'))
conf_data.set('SOURCE_DATE_EPOCH', get_option('source-date-epoch'))

luapkg = get_option('luapkg')
if luapkg != ''
    conf_data.set('CONFIG_SCRIPTING_LUA', true)
else
    conf_data.set('CONFIG_SCRIPTING_LUA', false)
endif

conf_data.set('CONFIG_TRE', get_option('tre'))
conf_data.set('CONFIG_SCRIPTING_RUBY', get_option('ruby'))
conf_data.set('CONFIG_GNUTLS', get_option('gnutls'))
conf_data.set('CONFIG_OPENSSL', get_option('openssl'))
conf_data.set('CONFIG_LIBEV', get_option('libev'))
conf_data.set('CONFIG_LIBEVENT', get_option('libevent'))
conf_data.set('CONFIG_LIBUV', get_option('libuv'))
conf_data.set('CONFIG_X', get_option('x'))
conf_data.set('CONFIG_QUICKJS', get_option('quickjs'))
conf_data.set('CONFIG_MUJS', get_option('mujs'))
conf_data.set('CONFIG_CODEPOINT', get_option('codepoint'))

if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY') or conf_data.get('CONFIG_ECMASCRIPT_SMJS') or conf_data.get('CONFIG_QUICKJS') or conf_data.get('CONFIG_MUJS')
    conf_data.set('CONFIG_ECMASCRIPT', true)
else
    conf_data.set('CONFIG_ECMASCRIPT', false)
endif

if conf_data.get('CONFIG_GETTEXT') and not conf_data.get('CONFIG_NLS')
    error('nls must be enabled too')
endif

if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY') and not conf_data.get('CONFIG_ECMASCRIPT_SMJS')
    error('spidermonkey must be enabled too')
endif

if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY') and conf_data.get('CONFIG_QUICKJS')
    error('quickjs and sm-scripting cannot be both enabled')
endif

if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY') and conf_data.get('CONFIG_MUJS')
    error('mujs and sm-scripting cannot be both enabled')
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS') and conf_data.get('CONFIG_QUICKJS')
    error('quickjs and spidermonkey cannot be both enabled')
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS') and conf_data.get('CONFIG_MUJS')
    error('mujs and spidermonkey cannot be both enabled')
endif

if conf_data.get('CONFIG_QUICKJS') and conf_data.get('CONFIG_MUJS')
    error('mujs and quickjs cannot be both enabled')
endif

if conf_data.get('CONFIG_LIBEV') and conf_data.get('CONFIG_LIBEVENT')
    error('libev and libevent cannot be both enabled')
endif

if conf_data.get('CONFIG_LIBEV') and conf_data.get('CONFIG_LIBUV')
    error('libev and libuv cannot be both enabled')
endif

if conf_data.get('CONFIG_LIBEVENT') and conf_data.get('CONFIG_LIBUV')
    error('libevent and libuv cannot be both enabled')
endif

if conf_data.get('CONFIG_FSP') and conf_data.get('CONFIG_FSP2')
    error('fsp and fsp2 cannot be both enabled')
endif

conf_data.set('CONFIG_ECMASCRIPT_SMJS_HEARTBEAT', true)

conf_data.set('CONFIG_SCRIPTING', true)

system = host_machine.system()

if system == 'haiku'
    conf_data.set('CONFIG_OS_BEOS', true)
else
    conf_data.set('CONFIG_OS_BEOS', false)
endif

if system == 'windows'
    conf_data.set('CONFIG_OS_WIN32', true)
else
    conf_data.set('CONFIG_OS_WIN32', false)
endif

if system == 'dos'
    conf_data.set('CONFIG_OS_DOS', true)
else
    conf_data.set('CONFIG_OS_DOS', false)
endif

if system != 'dos' and system != 'haiku' and system != 'windows'
    conf_data.set('CONFIG_OS_UNIX', true)
else
    conf_data.set('CONFIG_OS_UNIX', false)
endif

if conf_data.get('CONFIG_GNUTLS') or conf_data.get('CONFIG_OPENSSL')
    conf_data.set('CONFIG_SSL', true)
else
    conf_data.set('CONFIG_SSL', false)
endif

conf_data.set('CONFIG_OS_OS2', false)
conf_data.set('CONFIG_OS_RISCOS', false)

# AC_HEADER_DIRENT
conf_data.set('HAVE_DIRENT_H', 1)


# AC_HEADER_TIME
conf_data.set('HAVE_SYS_TIME_H', 1)

compiler = meson.get_compiler('c')

extracflags = []
extracppflags = []

if conf_data.get('CONFIG_ECMASCRIPT')
    extracppflags += ['-fpermissive', '-Wno-sign-compare']
endif

if compiler.has_header('argz.h')
    conf_data.set('HAVE_ARGZ_H', 1)
endif

if compiler.has_header('sys/wait.h')
    conf_data.set('HAVE_SYS_WAIT_H', 1)
endif

if compiler.has_header('wchar.h')
    conf_data.set('HAVE_WCHAR_H', 1)
endif

if compiler.has_header('wctype.h')
    conf_data.set('HAVE_WCTYPE_H', 1)
endif

if compiler.has_header('fcntl.h')
    conf_data.set('HAVE_FCNTL_H', 1)
endif

if compiler.has_header('libgen.h')
    conf_data.set('HAVE_LIBGEN_H', 1)
endif

if compiler.has_header('unistd.h')
    conf_data.set('HAVE_UNISTD_H', 1)
endif

if compiler.has_header('arpa/inet.h')
    conf_data.set('HAVE_ARPA_INET_H', 1)
endif

if compiler.has_header('netinet/in_systm.h')
    conf_data.set('HAVE_NETINET_IN_SYSTM_H', 1)
endif

if compiler.has_header('netinet/in_system.h')
    conf_data.set('HAVE_NETINET_IN_SYSTEM_H', 1)
endif

if compiler.has_header('netinet/ip.h')
    conf_data.set('HAVE_NETINET_IP_H', 1)
endif

if compiler.has_header('netdb.h')
    conf_data.set('HAVE_NETDB_H', 1)
endif

if compiler.has_header('netinet/in.h')
    conf_data.set('HAVE_NETINET_IN_H', 1)
endif

if compiler.has_header('netinet/in6_var.h')
    conf_data.set('HAVE_NETINET_IN6_VAR_H', 1)
endif

if compiler.has_header('ifaddrs.h')
    conf_data.set('HAVE_IFADDRS_H', 1)
endif

if compiler.has_header('sys/cygwin.h')
    conf_data.set('HAVE_SYS_CYGWIN_H', 1)
endif

if compiler.has_header('io.h')
    conf_data.set('HAVE_IO_H', 1)
endif

if compiler.has_header('sys/fmutex.h')
    conf_data.set('HAVE_SYS_FMUTEX_H', 1)
endif

if compiler.has_header('sys/ioctl.h')
    conf_data.set('HAVE_SYS_IOCTL_H', 1)
endif

if compiler.has_header('sys/kd.h')
    conf_data.set('HAVE_SYS_KD_H', 1)
endif

if compiler.has_header('sys/sockio.h')
   conf_data.set('HAVE_SYS_SOCKIO_H', 1)
endif

if compiler.has_header('sys/resource.h')
    conf_data.set('HAVE_SYS_RESOURCE_H', 1)
endif

if compiler.has_header('sys/select.h')
    conf_data.set('HAVE_SYS_SELECT_H', 1)
endif

if compiler.has_header('sys/socket.h')
    conf_data.set('HAVE_SYS_SOCKET_H', 1)
endif

if compiler.has_header('sys/utsname.h')
    conf_data.set('HAVE_SYS_UTSNAME_H', 1)
endif

if compiler.has_header('net/if.h')
   conf_data.set('HAVE_NET_IF_H', 1)
endif

if compiler.has_header('stdint.h')
    conf_data.set('HAVE_STDINT_H', 1)
endif

if compiler.has_header('inttypes.h')
    conf_data.set('HAVE_INTTYPES_H', 1)
endif

if compiler.has_header('pwd.h')
    conf_data.set('HAVE_PWD_H', 1)
endif

if compiler.has_header('termios.h')
    conf_data.set('HAVE_TERMIOS_H', 1)
endif

if compiler.has_header('poll.h')
   conf_data.set('HAVE_POLL_H', 1)
endif

if compiler.has_header('sys/types.h')
    conf_data.set('HAVE_SYS_TYPES_H', 1)
endif

if compiler.has_header('gpm.h')
    conf_data.set('HAVE_GPM_H', 1)
endif

if compiler.has_header('idn2.h')
    conf_data.set('HAVE_IDN2_H', 1)
endif

if compiler.has_header('event2/event.h')
    conf_data.set('HAVE_EVENT2_EVENT_H', 1)
endif

if compiler.has_header('event.h')
    conf_data.set('HAVE_EVENT_H', 1)
endif

if compiler.has_header('ev-event.h')
    conf_data.set('HAVE_EV_EVENT_H', 1)
endif

if compiler.has_header('uv.h')
    conf_data.set('HAVE_UV_H', 1)
endif

if compiler.has_header('alloca.h')
    conf_data.set('HAVE_ALLOCA_H', 1)
endif

if compiler.has_header('lauxlib.h')
    conf_data.set('HAVE_LAUXLIB_H', 1)
endif

if compiler.has_header('machine/console.h')
    conf_data.set('HAVE_MACHINE_CONSOLE_H', 1)
endif

if compiler.has_header('malloc.h')
    conf_data.set('HAVE_MALLOC_H', 1)
endif

if compiler.has_header('stdalign.h')
    conf_data.set('HAVE_STDALIGN_H', 1)
endif

if compiler.has_header('sys/consio.h')
    conf_data.set('HAVE_SYS_CONSIO_H', 1)
endif

if compiler.has_header('sys/param.h')
    conf_data.set('HAVE_SYS_PARAM_H', 1)
endif

if compiler.has_header('term.h')
    conf_data.set('HAVE_TERM_H', 1)
endif

if compiler.has_header('execinfo.h')
    conf_data.set('HAVE_EXECINFO_H', 1)
endif

if compiler.has_header('sys/eventfd.h')
    conf_data.set('HAVE_SYS_EVENTFD_H', 1)
endif

if conf_data.get('CONFIG_OS_WIN32') and compiler.has_header('windows.h')
    conf_data.set('HAVE_WINDOWS_H', 1)
endif

if conf_data.get('CONFIG_OS_WIN32') and compiler.has_header('winsock2.h')
    conf_data.set('HAVE_WINSOCK2_H', 1)
endif

if conf_data.get('CONFIG_OS_WIN32') and compiler.has_header('ws2tcpip.h')
    conf_data.set('HAVE_WS2TCPIP_H', 1)
endif

st = get_option('static')
deps = []

if conf_data.get('CONFIG_BACKTRACE') and conf_data.get('HAVE_EXECINFO_H') == 1
    execinfodeps = compiler.find_library('execinfo', static: st, required: false)
    if execinfodeps.found()
        deps += execinfodeps
    endif
endif

if conf_data.get('CONFIG_GZIP')
    zdeps = dependency('zlib', static: st, required: false)
    if not zdeps.found()
        zdeps = compiler.find_library('z', static: st)
    endif
    if zdeps.found()
        deps += zdeps
    endif
endif

if conf_data.get('CONFIG_TRE')
    tredeps = dependency('tre', static: st)
    deps += tredeps
endif

eh = ''
if conf_data.get('CONFIG_LIBEV')
    conf_data.set('HAVE_LIBEV_EVENT_H', compiler.has_header('libev/event.h'))
    conf_data.set('HAVE_LIBEV', true)
    eh = '#include <libev/event.h>'
    eventdeps = compiler.find_library('ev', static: st)
    deps += eventdeps
    conf_data.set('CONFIG_LIBEVENT', false)
    code1 = '''#include <libev/event.h>
    struct ev_timer timer;
    '''
    code2 = '''#include <event.h>
    struct ev_timer timer;
    '''
    if not compiler.compiles(code1) and not compiler.compiles(code2)
        error('libev cannot be used. Likely not installed libev-libevent-dev')
    endif
elif conf_data.get('CONFIG_LIBEVENT')
    eh = '#include <event2/event.h>'
    eventdeps = dependency('libevent', static: st, version: '>=2.0.0')
    deps += eventdeps
    conf_data.set('HAVE_LIBEVENT', true)
elif conf_data.get('CONFIG_LIBUV')
    eventdeps = dependency('libuv', static: st)
    deps += eventdeps
else
    eventdeps = []
endif

gnutlsdeps = []
ssldeps = []

if conf_data.get('CONFIG_OPENSSL')
    ssldeps = dependency('openssl', static: st, required: false)
    if not ssldeps.found()
        ssldeps = compiler.find_library('ssl', static: st)
    endif
    deps += ssldeps
    conf_data.set('USE_OPENSSL', true)
    conf_data.set('CONFIG_GNUTLS', false)
elif conf_data.get('CONFIG_GNUTLS')
    gnutlsdeps = dependency('gnutls', static: st)
    libgcryptdeps = dependency('libgcrypt', static: st)
    deps += gnutlsdeps
    deps += libgcryptdeps
endif

if conf_data.get('CONFIG_GSSAPI')
    gssapideps = dependency('krb5-gssapi', static: st)
    deps += gssapideps
endif

if conf_data.get('CONFIG_BROTLI')
    brotlideps = dependency('libbrotlidec', static: st)
    deps += brotlideps
endif

if conf_data.get('CONFIG_ZSTD')
    zstatic = st
    if conf_data.get('CONFIG_DEBUG')
        zstatic = true
    endif
    zstddeps = dependency('libzstd', static: zstatic)
    deps += zstddeps
endif

if conf_data.get('CONFIG_LZMA')
    lzmadeps = dependency('liblzma', static: st)
    deps += lzmadeps
endif

if conf_data.get('CONFIG_IDN2')
    idndeps = dependency('libidn2', static: st)
    deps += idndeps
endif

x11deps = []
conf_data.set('HAVE_X11', false)
if conf_data.get('CONFIG_X')
    x11deps = dependency('x11', static: st)
    deps += x11deps
    conf_data.set('HAVE_X11', compiler.has_header('X11/Xlib.h'))
endif

if conf_data.get('CONFIG_BZIP2')
    bz2deps = dependency('bzip2', static: st, required: false)
    if not bz2deps.found()
        bz2deps = compiler.find_library('bz2', static: st)
    endif
    deps += bz2deps
endif

mozjsdeps = []
if conf_data.get('CONFIG_ECMASCRIPT_SMJS') or conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY')
    mozjsdeps = dependency('mozjs-128', static: st)
    deps += mozjsdeps
endif

if conf_data.get('CONFIG_ECMASCRIPT')
    sqlite3deps = dependency('sqlite3', static: st)
    deps += sqlite3deps
endif

conf_data.set('CONFIG_LIBCURL', false)
curldeps = []

if not conf_data.get('CONFIG_LIBCURL') and get_option('libcurl')
    curldeps = dependency('libcurl', static: st, version: '>=7.66.0', required: false)
    if not curldeps.found()
        curldeps = compiler.find_library('curl', static: st)
    endif
    if curldeps.found()
        deps += curldeps
        conf_data.set('CONFIG_LIBCURL', true)
    endif
endif

conf_data.set('CONFIG_LIBCSS', false)
cssdeps = false

if conf_data.get('CONFIG_ECMASCRIPT')
    cssdeps = dependency('libcss', static: st, version: '>=0.9.2', required: false)
    if not cssdeps.found()
        cssdeps = dependency('libcss', static: true, version: '>=0.9.2')
    endif
    deps += cssdeps
    conf_data.set('CONFIG_LIBCSS', true)
endif

conf_data.set('CONFIG_LIBDOM', false)
libdomdeps = []

if conf_data.get('CONFIG_ECMASCRIPT')
    libdomdeps = dependency('libdom', static: st, version: '>=0.4.2', required: false)
    if not libdomdeps.found()
        libdomdeps = dependency('libdom', static: true, version: '>=0.4.2')
    endif
    deps += libdomdeps
    conf_data.set('CONFIG_LIBDOM', true)
endif

if not conf_data.get('CONFIG_LIBCSS') and get_option('libcss')
    cssdeps = dependency('libcss', static: st, version: '>=0.9.2', required: false)
    if not cssdeps.found()
        cssdeps = dependency('libcss', static: true, version: '>=0.9.2')
    endif
    deps += cssdeps
    conf_data.set('CONFIG_LIBCSS', true)
    if not conf_data.get('CONFIG_LIBDOM')
        libdomdeps = dependency('libdom', static: st, version: '>=0.4.2', required: false)
        if not libdomdeps.found()
            libdomdeps = dependency('libdom', static: true, version: '>=0.4.2')
        endif
        deps += libdomdeps
        conf_data.set('CONFIG_LIBDOM', true)
    endif
endif

if conf_data.get('CONFIG_SCRIPTING_LUA')
    luadeps = dependency(luapkg, static: st, required: false)
    if not luadeps.found()
        luadeps = compiler.find_library(luapkg, static: st, required: not conf_data.get('CONFIG_OS_DOS'))
    endif
    if not luadeps.found() and conf_data.get('CONFIG_OS_DOS')
        luadeps = compiler.find_library(luapkg, dirs: ['/home/elinks/lib'], static: st)
    endif
    deps += luadeps
endif

if conf_data.get('CONFIG_XBEL_BOOKMARKS')
    expatdeps = dependency('expat', static: st, required: false)
    if not expatdeps.found()
        expatdeps = compiler.find_library('expat', static: st, required: not conf_data.get('CONFIG_OS_DOS'))
    endif
    if not expatdeps.found() and conf_data.get('CONFIG_OS_DOS')
        expatdeps = compiler.find_library('expat', dirs: ['/home/elinks/lib'], static: st)
    endif
    deps += expatdeps
endif

if conf_data.get('CONFIG_GPM')
    libgpmdeps = compiler.find_library('gpm', static: st)
    deps += libgpmdeps
endif

if conf_data.get('CONFIG_SCRIPTING_PYTHON')
    python3deps = dependency('python3-embed', static: st)
    deps += python3deps
endif

if conf_data.get('CONFIG_SCRIPTING_PERL')
    perl_libs = ''
    perl_l = run_command('perl', '-MExtUtils::Embed', '-e', 'ldopts', check:true)
    if perl_l.returncode() == 0
        perl_libs = perl_l.stdout().strip().split()
    endif

    perl_cflags = ''
    perl_c = run_command('perl', '-MExtUtils::Embed', '-e', 'ccopts', check:true)
    if perl_c.returncode() == 0
        perl_cflags = perl_c.stdout().strip().split()
    endif
    perl_dep = declare_dependency(compile_args: perl_cflags, link_args: perl_libs)
    deps += perl_dep
endif

conf_data.set('CONFIG_PERL_POPPX_WITHOUT_N_A', 1)

rubydeps = []
if conf_data.get('CONFIG_SCRIPTING_RUBY')
    rubydeps = dependency('ruby', static: st)
    deps += rubydeps
endif

guiledeps = []
if conf_data.get('CONFIG_SCRIPTING_GUILE')
    guiledeps = dependency('guile-3.0', static: st)
    deps += guiledeps
endif

if conf_data.get('CONFIG_FSP')
    fspdeps = compiler.find_library('fsplib', static: st)
    deps += fspdeps
endif

if conf_data.get('CONFIG_SMB')
    smbdeps = dependency('smbclient', static: st)
    deps += smbdeps
endif

dep_atomic = dependency('', required: false)

if not compiler.links('''#include <stdint.h>
    int main() {
        struct {
            uint64_t *v;
        } x;
        return (int)__atomic_load_n(x.v, __ATOMIC_ACQUIRE) & (int)__atomic_add_fetch(x.v, (uint64_t)1, __ATOMIC_ACQ_REL);
    }''',
    name: 'GCC atomic builtins required -latomic')
    dep_atomic = compiler.find_library('atomic', static: st, required: false)
endif

if conf_data.get('CONFIG_QUICKJS')
    quickjsdeps = compiler.find_library('quickjs', static: st, required: false)
    if not quickjsdeps.found()
        quickjsdeps = compiler.find_library('quickjs/libquickjs', static: true, required: not conf_data.get('CONFIG_OS_DOS'))
    endif
    if not quickjsdeps.found() and conf_data.get('CONFIG_OS_DOS')
        quickjsdeps = compiler.find_library('quickjs/libquickjs', dirs: ['/home/elinks/lib'], static: true)
    endif
    deps += quickjsdeps
    deps += dep_atomic
endif

if conf_data.get('CONFIG_MUJS')
    mujsdeps = dependency('mujs', static: st)
    deps += mujsdeps
endif

if not conf_data.get('CONFIG_OS_DOS') and compiler.has_header('sys/un.h')
    conf_data.set('CONFIG_INTERLINK', true)
    conf_data.set('HAVE_SYS_UN_H', 1)
else
    conf_data.set('CONFIG_INTERLINK', false)
endif

if conf_data.get('CONFIG_TERMINFO')
    terminfodeps = dependency('ncursesw', static: st)
    deps += terminfodeps
endif

if conf_data.get('CONFIG_KITTY')
    if not conf_data.get('CONFIG_LIBDOM')
        libdomdeps = dependency('libdom', static: st, version: '>=0.4.2', required: false)
        if not libdomdeps.found()
            libdomdeps = dependency('libdom', static: true, version: '>=0.4.2')
        endif
        deps += libdomdeps
        conf_data.set('CONFIG_LIBDOM', true)
    endif
endif

if conf_data.get('CONFIG_LIBSIXEL')
    sixeldeps = dependency('libsixel', static: st)
    deps += sixeldeps
    if not conf_data.get('CONFIG_LIBDOM')
        libdomdeps = dependency('libdom', static: st, version: '>=0.4.2', required: false)
        if not libdomdeps.found()
            libdomdeps = dependency('libdom', static: true, version: '>=0.4.2')
        endif
        deps += libdomdeps
        conf_data.set('CONFIG_LIBDOM', true)
    endif
endif

if conf_data.get('CONFIG_LIBWEBP')
    webpdeps = dependency('libwebp', static: st)
    deps += webpdeps
endif

if conf_data.get('CONFIG_LIBAVIF')
    avifdeps = dependency('libavif', static: st)
    deps += avifdeps
endif

if conf_data.get('CONFIG_LIBWEBP') and not conf_data.get('CONFIG_KITTY') and not conf_data.get('CONFIG_LIBSIXEL')
    error('kitty or libsixel must be enabled for libwebp support')
endif

if conf_data.get('CONFIG_LIBAVIF') and not conf_data.get('CONFIG_KITTY') and not conf_data.get('CONFIG_LIBSIXEL')
    error('kitty or libsixel must be enabled for libavif support')
endif

if conf_data.get('CONFIG_OS_DOS')
    wattdeps = compiler.find_library('watt', dirs: ['/home/elinks/lib'], static: st)
    deps += wattdeps
else
    wattdeps = []
endif

#AC_STRUCT_TM
#AC_C_CONST
#AC_C_INLINE

if conf_data.get('CONFIG_ECMASCRIPT')
    conf_data.set('NONSTATIC_INLINE', '')
else
    conf_data.set('NONSTATIC_INLINE', 'inline')
endif

#AC_SYS_LARGEFILE

#AC_TYPE_SIZE_T
#AC_TYPE_OFF_T
#EL_CHECK_TYPE(ssize_t, int)

conf_data.set('HAVE_LONG_LONG', 1)
conf_data.set('HAVE_OFF_T', 1)
conf_data.set('HAVE_INT32_T', 1)
conf_data.set('HAVE_UINT32_T', 1)
conf_data.set('HAVE_UINT16_T', 1)
conf_data.set('HAVE_VARIADIC_MACROS', 1)

#AC_FUNC_MEMCMP
#AC_FUNC_MMAP

if compiler.has_function('_beginthread', prefix: '#include <stdlib.h>')
    conf_data.set('HAVE_BEGINTHREAD', 1)
endif

if compiler.has_function('cygwin_conv_to_full_win32_path', prefix: '#include <stdlib.h>')
    conf_data.set('HAVE_CYGWIN_CONV_TO_FULL_WIN32_PATH', 1)
endif

if compiler.has_function('feof_unlocked', prefix: '#include <stdio.h>')
    conf_data.set('HAVE_FEOF_UNLOCKED', 1)
endif

if compiler.has_function('fgets_unlocked', prefix: '#include <stdio.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_FGETS_UNLOCKED', 1)
endif

if compiler.has_function('mmap', prefix: '#include <sys/mman.h>')
    conf_data.set('HAVE_MMAP', 1)
endif

if compiler.has_function('MouOpen', prefix: '#include <stdlib.h>')
    conf_data.set('HAVE_MOUOPEN', 1)
endif

if compiler.has_function('munmap', prefix: '#include <sys/mman.h>')
    conf_data.set('HAVE_MUNMAP', 1)
endif

if compiler.has_function('_read_kbd', prefix: '#include <stdlib.h>')
    conf_data.set('HAVE_READ_KBD', 1)
endif

if compiler.has_function('strftime', prefix: '#include <time.h>')
    conf_data.set('HAVE_STRFTIME', 1)
endif

if compiler.has_function('strptime', prefix: '#include <time.h>', args: '-D_XOPEN_SOURCE')
    conf_data.set('HAVE_STRPTIME', 1)
endif

if compiler.has_function('tsearch', prefix: '#include <search.h>')
    conf_data.set('HAVE_TSEARCH', 1)
endif

if compiler.has_function('__argz_count', prefix: '#include <argz.h>')
    conf_data.set('HAVE___ARGZ_COUNT', 1)
endif

if compiler.has_function('__argz_next', prefix: '#include <argz.h>')
    conf_data.set('HAVE___ARGZ_NEXT', 1)
endif

if compiler.has_function('__argz_stringify', prefix: '#include <argz.h>')
    conf_data.set('HAVE___ARGZ_STRINGIFY', 1)
endif

if compiler.has_function('atoll', prefix : '#include <stdlib.h>')
    conf_data.set('HAVE_ATOLL', 1)
endif

if compiler.has_function('gethostbyaddr', prefix : '#include <netdb.h>')
    conf_data.set('HAVE_GETHOSTBYADDR', 1)
endif

if compiler.has_function('herror', prefix : '#include <netdb.h>')
    conf_data.set('HAVE_HERROR', 1)
endif

if compiler.has_function('strerror', prefix : '#include <string.h>')
    conf_data.set('HAVE_STRERROR', 1)
endif

if compiler.has_function('popen', prefix : '#include <stdio.h>')
    conf_data.set('HAVE_POPEN', 1)
endif

if compiler.has_function('uname', prefix : '#include <sys/utsname.h>')
    conf_data.set('HAVE_UNAME', 1)
endif

if compiler.has_function('access', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_ACCESS', 1)
endif

if compiler.has_function('chmod', prefix : '#include <sys/stat.h>')
    conf_data.set('HAVE_CHMOD', 1)
endif

if compiler.has_function('alarm', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_ALARM', 1)
endif

if compiler.has_function('timegm', prefix : '#include <time.h>')
    conf_data.set('HAVE_TIMEGM', 1)
endif

if compiler.has_function('mremap', prefix : '#include <sys/mman.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_MREMAP', 1)
endif

if compiler.has_function('strcasecmp', prefix : '#include <strings.h>')
    conf_data.set('HAVE_STRCASECMP', 1)
endif

if compiler.has_function('strncasecmp', prefix : '#include <strings.h>')
    conf_data.set('HAVE_STRNCASECMP', 1)
endif

if compiler.has_function('strcasestr', prefix : '#include <string.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_STRCASESTR', 1)
endif

conf_data.set('HAVE_STRSTR', 1)
conf_data.set('HAVE_STRCHR', 1)
conf_data.set('HAVE_STRRCHR', 1)

if compiler.has_function('memmove', prefix : '#include <string.h>')
    conf_data.set('HAVE_MEMMOVE', 1)
endif

if compiler.has_function('bcopy', prefix : '#include <strings.h>')
    conf_data.set('HAVE_BCOPY', 1)
endif

if compiler.has_function('stpcpy', prefix : '#include <string.h>')
    conf_data.set('HAVE_STPCPY', 1)
endif

if compiler.has_function('strdup', prefix : '#include <string.h>')
    conf_data.set('HAVE_STRDUP', 1)
endif

if compiler.has_function('index', prefix : '#include <strings.h>')
    conf_data.set('HAVE_INDEX', 1)
endif

if compiler.has_function('isdigit', prefix : '#include <ctype.h>')
    conf_data.set('HAVE_ISDIGIT', 1)
endif

if compiler.has_function('mempcpy', prefix : '#include <string.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_MEMPCPY', 1)
endif

if compiler.has_function('memrchr', prefix : '#include <string.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_MEMRCHR', 1)
endif

if compiler.has_function('snprintf', prefix : '#include <stdio.h>')
    conf_data.set('HAVE_SNPRINTF', 1)
endif

if compiler.has_function('vsnprintf', prefix : '#include <stdio.h>\n#include <stdarg.h>', args: '-D_ISOC99_SOURCE')
    conf_data.set('HAVE_VSNPRINTF', 1)
    conf_data.set('HAVE_C99_VSNPRINTF', 1)
endif

if compiler.has_function('asprintf', prefix : '#include <stdio.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_ASPRINTF', 1)
endif

if compiler.has_function('vasprintf', prefix : '#include <stdio.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_VASPRINTF', 1)
endif

if compiler.has_function('getifaddrs', prefix : '#include <ifaddrs.h>')
    conf_data.set('HAVE_GETIFADDRS', 1)
endif

if compiler.has_function('getpwnam', prefix : '#include <pwd.h>')
    conf_data.set('HAVE_GETPWNAM', 1)
endif

if compiler.has_function('inet_aton', prefix : '#include <arpa/inet.h>', dependencies: wattdeps)
    conf_data.set('HAVE_INET_ATON', 1)
endif

if compiler.has_function('inet_pton', prefix : '#include <arpa/inet.h>', dependencies: wattdeps)
    conf_data.set('HAVE_INET_PTON', 1)
endif

if compiler.has_function('inet_ntop', prefix : '#include <arpa/inet.h>', dependencies: wattdeps)
    conf_data.set('HAVE_INET_NTOP', 1)
elif compiler.has_function('inet_ntop', prefix : '#include <ws2tcpip.h>')
    conf_data.set('HAVE_INET_NTOP', 1)
endif

if compiler.has_function('fflush', prefix : '#include <stdio.h>')
    conf_data.set('HAVE_FFLUSH', 1)
endif

if compiler.has_function('fsync', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_FSYNC', 1)
endif

if compiler.has_function('fseeko', prefix : '#include <stdio.h>')
    conf_data.set('HAVE_FSEEKO', 1)
endif

if compiler.has_function('ftello', prefix : '#include <stdio.h>')
    conf_data.set('HAVE_FTELLO', 1)
endif

if compiler.has_function('sigaction', prefix : '#include <signal.h>')
    conf_data.set('HAVE_SIGACTION', 1)
endif

if compiler.has_function('gettimeofday', prefix : '#include <sys/time.h>')
    conf_data.set('HAVE_GETTIMEOFDAY', 1)
endif

if compiler.has_function('clock_gettime', prefix : '#include <time.h>')
    conf_data.set('HAVE_CLOCK_GETTIME', 1)
endif

if compiler.has_function('setenv', prefix : '#include <stdlib.h>')
    conf_data.set('HAVE_SETENV', 1)
    conf_data.set('HAVE_SETENV_OR_PUTENV', true)
endif

if compiler.has_function('putenv', prefix : '#include <stdlib.h>')
    conf_data.set('HAVE_PUTENV', 1)
    conf_data.set('HAVE_SETENV_OR_PUTENV', true)
endif

if compiler.has_function('unsetenv', prefix : '#include <stdlib.h>')
    conf_data.set('HAVE_UNSETENV', 1)
endif

if compiler.has_function('getegid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETEGID', 1)
endif

if compiler.has_function('getgid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETGID', 1)
endif

if compiler.has_function('getuid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETUID', 1)
endif

if compiler.has_function('geteuid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETEUID', 1)
endif

if compiler.has_function('wcwidth', prefix : '#include <wchar.h>', args: '-D_XOPEN_SOURCE')
    conf_data.set('HAVE_WCWIDTH', 1)
endif

if not conf_data.get('CONFIG_OS_DOS') and compiler.has_function('fork', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_FORK', 1)
endif

if compiler.has_function('setpgid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_SETPGID', 1)
endif

if compiler.has_function('getpgid', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETPGID', 1)
endif

if compiler.has_function('setpgrp', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_SETPGRP', 1)
endif

if compiler.has_function('getpgrp', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_GETPGRP', 1)
endif

if compiler.has_function('raise', prefix : '#include <signal.h>')
    conf_data.set('HAVE_RAISE', 1)
endif

if compiler.has_function('kill', prefix : '#include <signal.h>')
    conf_data.set('HAVE_KILL', 1)
endif

if compiler.has_function('fpathconf', prefix : '#include <unistd.h>')
    conf_data.set('HAVE_FPATHCONF', 1)
endif

if compiler.has_function('poll', prefix : '#include <poll.h>')
    conf_data.set('HAVE_POLL', 1)
endif

if compiler.has_function('event_base_set', prefix: eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_BASE_SET', 1)
endif

if compiler.has_function('event_get_version', prefix: eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_GET_VERSION', 1)
endif

if compiler.has_function('event_get_method', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_GET_METHD', 1)
endif

if compiler.has_function('event_base_free', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_BASE_FREE', 1)
endif

if compiler.has_function('event_base_new', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_BASE_NEW', 1)
endif

if compiler.has_function('event_reinit', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_REINIT', 1)
endif

if compiler.has_function('event_base_get_method', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_BASE_GET_METHOD', 1)
endif

if compiler.has_function('event_reinit', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_REINIT', 1)
endif

if compiler.has_function('event_config_set_flag', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_CONFIG_SET_FLAG', 1)
endif

if compiler.has_function('event_get_struct_event_size', prefix : eh, dependencies: eventdeps)
    conf_data.set('HAVE_EVENT_GET_STRUCT_EVENT_SIZE', 1)
endif

if compiler.has_function('RAND_add', prefix: '#include <openssl/rand.h>', dependencies: ssldeps)
    conf_data.set('HAVE_RAND_ADD', 1)
endif

if compiler.has_function('RAND_bytes', prefix: '#include <openssl/rand.h>', dependencies: ssldeps)
    conf_data.set('HAVE_RAND_BYTES', 1)
endif

if compiler.has_function('ASN1_STRING_get0_data', prefix: '#include <openssl/asn1.h>', dependencies: ssldeps)
    conf_data.set('HAVE_ASN1_STRING_GET0_DATA', 1)
endif

iconvdeps = compiler.find_library('iconv', static: st, required: false)
if iconvdeps.found()
    deps += iconvdeps
elif conf_data.get('CONFIG_OS_DOS')
    iconvdeps = compiler.find_library('libiconv', dirs: ['/home/elinks/lib'], static: st)
    deps += iconvdeps
endif

if compiler.has_function('nl_langinfo', prefix: '#include <langinfo.h>')
    conf_data.set('HAVE_LANGINFO_CODESET', 1)
endif

if compiler.has_function('iconv', prefix: '#include <iconv.h>', dependencies: iconvdeps)
    conf_data.set('HAVE_ICONV', 1)
endif

if conf_data.get('CONFIG_OS_DOS') or compiler.has_function('alloca', prefix: '#include <alloca.h>')
    conf_data.set('HAVE_ALLOCA', 1)
endif

if compiler.has_function('rb_errinfo', prefix: '#include <ruby.h>', dependencies: rubydeps)
    conf_data.set('HAVE_RB_ERRINFO', 1)
endif

if compiler.has_function('gnutls_priority_set_direct', prefix: '#include <gnutls/gnutls.h>', dependencies: gnutlsdeps)
    conf_data.set('HAVE_GNUTLS_PRIORITY_SET_DIRECT', 1)
endif

if compiler.has_function('gnutls_certificate_set_x509_system_trust', prefix: '#include <gnutls/gnutls.h>', dependencies: gnutlsdeps)
    conf_data.set('HAVE_GNUTLS_CERTIFICATE_SET_X509_SYSTEM_TRUST', 1)
endif

if compiler.has_function('mkstemps', prefix: '#include <stdlib.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_MKSTEMPS', 1)
endif

if compiler.has_function('setlocale', prefix: '#include <locale.h>')
    conf_data.set('HAVE_SETLOCALE', 1)
endif

if compiler.has_function('strtoul', prefix: '#include <stdlib.h>')
    conf_data.set('HAVE_STRTOUL', 1)
endif

if compiler.has_function('open_memstream', prefix: '#include <stdio.h>')
    conf_data.set('HAVE_OPEN_MEMSTREAM', 1)
endif

if compiler.compiles('int a; typeof(a) b;', name: 'typeof')
    conf_data.set('HAVE_TYPEOF', 1)
endif

if compiler.has_function('memfd_create', prefix: '#include <sys/mman.h>', args: '-D_GNU_SOURCE')
    conf_data.set('HAVE_MEMFD_CREATE', 1)
endif

if conf_data.get('CONFIG_GETTEXT')
    code = '''#include <libintl.h>
    extern int _nl_msg_cat_cntr;
    void main() { _nl_msg_cat_cntr = 1; }
    '''
    if compiler.links(code, name: 'test')
        conf_data.set('HAVE_NL_MSG_CAT_CNTR', 1)
    endif
endif

if compiler.compiles('''#include <unistd.h>
int func(void) { return sysconf(_SC_PAGE_SIZE); }
''', name: '_SC_PAGE_SIZE')
    conf_data.set('HAVE_SC_PAGE_SIZE', 1)
endif

if compiler.compiles('''#include <locale.h>
int func(void) { return LC_MESSAGES; }
''', name: 'LC_MESSAGES')
    conf_data.set('HAVE_LC_MESSAGES', 1)
endif

conf_data.set('ICONV_CONST', true)

sysconfdir = get_option('prefix') / get_option('sysconfdir')/'elinks'
conf_data.set('CONFDIR', sysconfdir)
conf_data.set('sysconfdir', sysconfdir)
conf_data.set('SOMETHING', '@SOMETHING@')
conf_data.set('api_srcdir', srcdir / 'src')
conf_data.set('LIBDIR', get_option('prefix') / 'lib')
conf_data.set('LOCALEDIR', get_option('prefix') / 'share/locale')

conf_data.set('HAVE_SA_STORAGE', true)
conf_data.set('HAVE_SA_IN6', true)

conf_data.set('SIZEOF_CHAR', compiler.sizeof('char'))
conf_data.set('SIZEOF_SHORT', compiler.sizeof('short'))
conf_data.set('SIZEOF_INT', compiler.sizeof('int'))
conf_data.set('SIZEOF_LONG', compiler.sizeof('long'))
conf_data.set('SIZEOF_LONG_LONG', compiler.sizeof('long long'))
conf_data.set('SIZEOF_OFF_T', compiler.sizeof('off_t'))
conf_data.set('SIZEOF_INTPTR_T', compiler.sizeof('intptr_t', prefix: '#include <stdint.h>'))

conf_data.set('CONFIG_GNUTLS_OPENSSL_COMPAT', false)

configure_file(input : 'config2.h.in',
               output : 'config.h',
               configuration : conf_data)

testdeps = files('src/intl/charsets.c', 'src/osdep/stub.c', 'src/util/conv.c', 'src/util/error.c',
'src/util/file.c', 'src/util/hash.c', 'src/util/memory.c', 'src/util/string.c', 'src/util/time.c')

if conf_data.get('CONFIG_DEBUG')
    testdeps += files('src/util/memdebug.c')
endif

if conf_data.get('CONFIG_UTF8')
    testdeps += files('src/intl/width.c')
endif

if not conf_data.get('CONFIG_SMALL')
    testdeps += files('src/util/fastfind.c')
endif

if conf_data.get('CONFIG_NLS')
    subdir('po')
endif
subdir('src')
subdir('contrib')

if get_option('doc')
    subdir('doc')
endif

if get_option('test') and get_option('test-js')
    subdir('test/js/assert')
endif
