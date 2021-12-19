project('elinks', ['c','cpp'], version:'0.15.0rc2', license:'GPLv2')
srcs=[]

srcdir = meson.source_root()

txt = run_command('git', '--git-dir=' + srcdir + '/.git', 'rev-parse', 'HEAD').stdout().strip()
dirty = run_command(srcdir + '/git-dirty.sh').stdout().strip()
add_global_arguments('-DBUILD_ID="' + txt + dirty + '"', language : 'c')

conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
conf_data.set('PACKAGE', meson.project_name())


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

conf_data.set('CONFIG_URI_REWRITE', get_option('uri-rewrite'))
conf_data.set('CONFIG_CGI', get_option('cgi'))
conf_data.set('CONFIG_FINGER', get_option('finger'))
conf_data.set('CONFIG_FSP', get_option('fsp'))
conf_data.set('CONFIG_FTP', get_option('ftp'))
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
conf_data.set('CONFIG_NO_ROOT', get_option('no-root'))
conf_data.set('CONFIG_DEBUG', get_option('withdebug'))
conf_data.set('CONFIG_FASTMEM', get_option('fastmem'))
conf_data.set('CONFIG_OWN_LIBC', get_option('own-libc'))
conf_data.set('CONFIG_SMALL', get_option('small'))
conf_data.set('CONFIG_UTF8', get_option('utf-8'))
conf_data.set('CONFIG_COMBINING', get_option('combining'))

conf_data.set('CONFIG_XTERM', get_option('xterm'))
conf_data.set('CONFIG_GPM', get_option('gpm'))
conf_data.set('CONFIG_TERMINFO', get_option('terminfo'))
conf_data.set('CONFIG_GZIP', get_option('zlib'))
conf_data.set('CONFIG_BZIP2', get_option('bzlib'))
conf_data.set('CONFIG_BROTLI', get_option('brotli'))

conf_data.set('CONFIG_ZSTD', get_option('zstd'))
conf_data.set('CONFIG_IDN', get_option('idn'))
conf_data.set('CONFIG_LZMA', get_option('lzma'))
conf_data.set('CONFIG_GSSAPI', get_option('gssapi'))
conf_data.set('CONFIG_ECMASCRIPT_SMJS', get_option('spidermonkey'))
conf_data.set('CONFIG_SCRIPTING_GUILE', get_option('guile'))
conf_data.set('CONFIG_SCRIPTING_PERL', get_option('perl'))
conf_data.set('CONFIG_SCRIPTING_PYTHON', get_option('python'))

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
conf_data.set('CONFIG_X', get_option('x'))
conf_data.set('CONFIG_XML', get_option('xml'))
conf_data.set('CONFIG_QUICKJS', get_option('quickjs'))

if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY') or conf_data.get('CONFIG_ECMASCRIPT_SMJS') or conf_data.get('CONFIG_QUICKJS')
    conf_data.set('CONFIG_ECMASCRIPT', true)
else
    conf_data.set('CONFIG_ECMASCRIPT', false)
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

if system != 'haiku' and system != 'windows'
    conf_data.set10('CONFIG_OS_UNIX', true)
else
    conf_data.set10('CONFIG_OS_UNIX', false)
endif

conf_data.set('CONFIG_SSL', true)
conf_data.set('CONFIG_OS_OS2', false)
conf_data.set('CONFIG_OS_RISCOS', false)

# AC_HEADER_DIRENT
conf_data.set10('HAVE_DIRENT_H', 1)
# AC_HEADER_SYS_WAIT
conf_data.set10('HAVE_SYS_WAIT_H', 1)
# AC_HEADER_TIME
conf_data.set10('HAVE_SYS_TIME_H', 1)

compiler = meson.get_compiler('c')

if conf_data.get('CONFIG_ECMASCRIPT')
    extracflags = ['-xc++', '-fpermissive']
else
    extracflags = []
endif

conf_data.set10('HAVE_WCHAR_H', compiler.has_header('wchar.h'))
conf_data.set10('HAVE_WCTYPE_H', compiler.has_header('wctype.h'))
conf_data.set10('HAVE_FCNTL_H', compiler.has_header('fcntl.h'))
conf_data.set10('HAVE_UNISTD_H', compiler.has_header('unistd.h'))
conf_data.set10('HAVE_SIGACTION_H', compiler.has_header('sigaction.h'))
conf_data.set10('HAVE_ARPA_INET_H', compiler.has_header('arpa/inet.h'))
conf_data.set10('HAVE_NETINET_IN_SYSTM_H', compiler.has_header('netinet/in_systm.h'))
conf_data.set10('HAVE_NETINET_IN_SYSTEM_H', compiler.has_header('netinet/in_system.h'))
conf_data.set10('HAVE_NETINET_IP_H', compiler.has_header('netinet/ip.h'))
conf_data.set10('HAVE_NETDB_H', compiler.has_header('netdb.h'))

conf_data.set10('HAVE_NETINET_IN_H', compiler.has_header('netinet/in.h'))
conf_data.set10('HAVE_NETINET_IN6_VAR_H', compiler.has_header('netinet/in6_var.h'))
conf_data.set10('HAVE_IFADDRS_H', compiler.has_header('ifaddrs.h'))
conf_data.set('HAVE_SYS_CYGWIN_H', compiler.has_header('sys/cygwin.h'))
conf_data.set('HAVE_IO_H', compiler.has_header('io.h'))
conf_data.set10('HAVE_SYS_FMUTEX_H', compiler.has_header('sys/fmutex.h'))
conf_data.set10('HAVE_SYS_IOCTL_H', compiler.has_header('sys/ioctl.h'))
conf_data.set10('HAVE_SYS_SOCKIO_H', compiler.has_header('sys/sockio.h'))
conf_data.set10('HAVE_SYS_RESOURCE_H', compiler.has_header('sys/resource.h'))
conf_data.set10('HAVE_SYS_SELECT_H', compiler.has_header('sys/select.h'))
conf_data.set10('HAVE_SYS_SOCKET_H', compiler.has_header('sys/socket.h'))
conf_data.set10('HAVE_SYS_TIME_H', compiler.has_header('sys/time.h'))
conf_data.set10('HAVE_SYS_UTSNAME_H', compiler.has_header('sys/utsname.h'))
conf_data.set10('HAVE_NET_IF_H', compiler.has_header('net/if.h'))

conf_data.set10('HAVE_STDINT_H', compiler.has_header('stdint.h'))
conf_data.set10('HAVE_INTTYPES_H', compiler.has_header('inttypes.h'))
conf_data.set10('HAVE_PWD_H', compiler.has_header('pwd.h'))
conf_data.set10('HAVE_TERMIOS_H', compiler.has_header('termios.h'))
conf_data.set10('HAVE_POLL_H', compiler.has_header('poll.h'))

conf_data.set10('HAVE_SYS_TYPES_H', compiler.has_header('sys/types.h'))
conf_data.set10('HAVE_GPM_H', compiler.has_header('gpm.h'))
conf_data.set('HAVE_IDNA_H', compiler.has_header('idna.h'))

conf_data.set('HAVE_EVENT_H', compiler.has_header('event.h'))

conf_data.set10('HAVE_ALLOCA_H', compiler.has_header('alloca.h'))

conf_data.set('HAVE_STDALIGN_H', compiler.has_header('stdalign.h'))

conf_data.set('HAVE_TERM_H', compiler.has_header('term.h'))

deps = []
if conf_data.get('CONFIG_GZIP')
    zdeps = dependency('zlib')
    deps += zdeps
endif

if conf_data.get('CONFIG_TRE')
    tredeps = dependency('tre')
    deps += tredeps
endif

eh = ''
if conf_data.get('CONFIG_LIBEV')
    conf_data.set('HAVE_LIBEV_EVENT_H', compiler.has_header('libev/event.h'))
    conf_data.set('HAVE_LIBEV', true)
    eh = '#include <libev/event.h>'
    eventdeps = compiler.find_library('ev')
    deps += eventdeps
    conf_data.set('CONFIG_LIBEVENT', false)
elif conf_data.get('CONFIG_LIBEVENT')
    eh = '#include <event.h>'
    eventdeps = dependency('libevent')
    deps += eventdeps
    conf_data.set('HAVE_LIBEVENT', true)
else
    eventdeps = []
endif

gnutlsdeps = []
ssldeps = []

if conf_data.get('CONFIG_OPENSSL')
    ssldeps = dependency('openssl')
    deps += ssldeps
    conf_data.set('USE_OPENSSL', true)
    conf_data.set('CONFIG_GNUTLS', false)
elif conf_data.get('CONFIG_GNUTLS')
    gnutlsdeps = dependency('gnutls')
    libgcryptdeps = dependency('libgcrypt')
    deps += gnutlsdeps
    deps += libgcryptdeps
endif

if conf_data.get('CONFIG_BROTLI')
    brotlideps = dependency('libbrotlidec')
    deps += brotlideps
endif

if conf_data.get('CONFIG_ZSTD')
    zstddeps = dependency('libzstd')
    deps += zstddeps
endif

if conf_data.get('CONFIG_LZMA')
    lzmadeps = dependency('liblzma')
    deps += lzmadeps
endif

if conf_data.get('CONFIG_IDN')
    idndeps = dependency('libidn')
    deps += idndeps
endif

conf_data.set('HAVE_X11', false)
if conf_data.get('CONFIG_X')
    x11deps = dependency('x11')
    deps += x11deps
    conf_data.set('HAVE_X11', compiler.has_header('X11/Xlib.h'))
endif

if conf_data.get('CONFIG_BZIP2')
    bz2deps = dependency('bzip2')
    deps += bz2deps
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
    mozjsdeps = dependency('mozjs-78')
    deps += mozjsdeps
endif

if conf_data.get('CONFIG_ECMASCRIPT')
    sqlite3deps = dependency('sqlite3')
    deps += sqlite3deps
    conf_data.set('CONFIG_XML', true)
endif

if conf_data.get('CONFIG_SCRIPTING_LUA')
    luadeps = dependency(luapkg)
    deps += luadeps
endif

if conf_data.get('CONFIG_XBEL_BOOKMARKS')
    expatdeps = dependency('expat')
    deps += expatdeps
endif

if conf_data.get('CONFIG_GPM')
    libgpmdeps = compiler.find_library('gpm')
    deps += libgpmdeps
endif

if conf_data.get('CONFIG_XML')
    xmlplusplusdeps = dependency('libxml++-5.0')
    deps += xmlplusplusdeps
endif

if conf_data.get('CONFIG_SCRIPTING_PYTHON')
    python3deps = dependency('python3-embed')
    deps += python3deps
endif

if conf_data.get('CONFIG_SCRIPTING_PERL')
    perl_libs = ''
    perl_l = run_command('perl', '-MExtUtils::Embed', '-e', 'ldopts')
    if perl_l.returncode() == 0
        perl_libs = perl_l.stdout().strip().split()
    endif

    perl_cflags = ''
    perl_c = run_command('perl', '-MExtUtils::Embed', '-e', 'ccopts')
    if perl_c.returncode() == 0
        perl_cflags = perl_c.stdout().strip().split()
    endif
    perl_dep = declare_dependency(compile_args: perl_cflags, link_args: perl_libs)
    deps += perl_dep
endif

rubydeps = []
if conf_data.get('CONFIG_SCRIPTING_RUBY')
    rubydeps = dependency('ruby')
    deps += rubydeps
endif

guiledeps = []
if conf_data.get('CONFIG_SCRIPTING_GUILE')
    guiledeps = dependency('guile-3.0')
    deps += guiledeps
endif

if conf_data.get('CONFIG_FSP')
    fspdeps = compiler.find_library('fsplib')
    deps += fspdeps
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
        dep_atomic = compiler.find_library('atomic')
endif

if conf_data.get('CONFIG_QUICKJS')
    quickjsdeps = compiler.find_library('quickjs/libquickjs', static : true)
    deps += quickjsdeps
    deps += dep_atomic
endif

if compiler.has_header('sys/un.h')
    conf_data.set('CONFIG_INTERLINK', true)
    conf_data.set10('HAVE_SYS_UN_H', true)
else
    conf_data.set('CONFIG_INTERLINK', false)
endif

if conf_data.get('CONFIG_TERMINFO')
    terminfodeps = dependency('ncursesw')
    deps += terminfodeps
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

conf_data.set10('HAVE_LONG_LONG', 1)
conf_data.set10('HAVE_OFF_T', 1)
conf_data.set10('HAVE_INT32_T', 1)
conf_data.set10('HAVE_UINT32_T', 1)
conf_data.set10('HAVE_UINT16_T', 1)
conf_data.set10('HAVE_VARIADIC_MACROS', 1)

#AC_FUNC_MEMCMP
#AC_FUNC_MMAP

conf_data.set10('HAVE_MMAP', 1)

if compiler.has_function('strftime', prefix: '#include <time.h>')
    conf_data.set10('HAVE_STRFTIME', 1)
endif

if compiler.has_function('strptime', prefix: '#include <time.h>', args: '-D_XOPEN_SOURCE')
    conf_data.set10('HAVE_STRPTIME', 1)
endif

if compiler.has_function('atoll', prefix : '#include <stdlib.h>')
    conf_data.set10('HAVE_ATOLL', 1)
endif

if compiler.has_function('gethostbyaddr', prefix : '#include <netdb.h>')
    conf_data.set10('HAVE_GETHOSTBYADDR', 1)
endif

if compiler.has_function('herror', prefix : '#include <netdb.h>')
    conf_data.set10('HAVE_HERROR', 1)
endif

if compiler.has_function('strerror', prefix : '#include <string.h>')
    conf_data.set10('HAVE_STRERROR', 1)
endif

if compiler.has_function('popen', prefix : '#include <stdio.h>')
    conf_data.set10('HAVE_POPEN', 1)
endif

if compiler.has_function('uname', prefix : '#include <sys/utsname.h>')
    conf_data.set10('HAVE_UNAME', 1)
endif

if compiler.has_function('access', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_ACCESS', 1)
endif

if compiler.has_function('chmod', prefix : '#include <sys/stat.h>')
    conf_data.set10('HAVE_CHMOD', 1)
endif

if compiler.has_function('alarm', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_ALARM', 1)
endif

if compiler.has_function('timegm', prefix : '#include <time.h>')
    conf_data.set10('HAVE_TIMEGM', 1)
endif

if compiler.has_function('mremap', prefix : '#include <sys/mman.h>', args: '-D_GNU_SOURCE')
    conf_data.set10('HAVE_MREMAP', 1)
endif

if compiler.has_function('strcasecmp', prefix : '#include <strings.h>')
    conf_data.set10('HAVE_STRCASECMP', 1)
endif

if compiler.has_function('strncasecmp', prefix : '#include <strings.h>')
    conf_data.set10('HAVE_STRNCASECMP', 1)
endif

if compiler.has_function('strcasestr', prefix : '#include <string.h>', args: '-D_GNU_SOURCE')
    conf_data.set10('HAVE_STRCASESTR', 1)
endif

conf_data.set10('HAVE_STRSTR', 1)
conf_data.set10('HAVE_STRCHR', 1)
conf_data.set10('HAVE_STRRCHR', 1)

if compiler.has_function('memmove', prefix : '#include <string.h>')
    conf_data.set10('HAVE_MEMMOVE', 1)
endif

if compiler.has_function('bcopy', prefix : '#include <strings.h>')
    conf_data.set10('HAVE_BCOPY', 1)
endif

if compiler.has_function('stpcpy', prefix : '#include <string.h>')
    conf_data.set10('HAVE_STPCPY', 1)
endif

if compiler.has_function('strdup', prefix : '#include <string.h>')
    conf_data.set10('HAVE_STRDUP', 1)
endif

if compiler.has_function('index', prefix : '#include <strings.h>')
    conf_data.set10('HAVE_INDEX', 1)
endif

if compiler.has_function('isdigit', prefix : '#include <ctype.h>')
    conf_data.set10('HAVE_ISDIGIT', 1)
endif

if compiler.has_function('mempcpy', prefix : '#include <string.h>')
    conf_data.set10('HAVE_MEMPCPY', 1)
endif

if compiler.has_function('memrchr', prefix : '#include <string.h>', args: '-D_GNU_SOURCE')
    conf_data.set10('HAVE_MEMRCHR', 1)
endif

if compiler.has_function('snprintf', prefix : '#include <stdio.h>')
    conf_data.set10('HAVE_SNPRINTF', 1)
endif

if compiler.has_function('vsnprintf', prefix : '#include <stdarg.h>')
    conf_data.set10('HAVE_VSNPRINTF', 1)
endif

if compiler.has_function('asprintf', prefix : '#include <stdio.h>', args: '-D_GNU_SOURCE')
    conf_data.set10('HAVE_ASPRINTF', 1)
endif

if compiler.has_function('vasprintf', prefix : '#include <stdio.h>', args: '-D_GNU_SOURCE')
    conf_data.set10('HAVE_VASPRINTF', 1)
endif

if compiler.has_function('getifaddrs', prefix : '#include <ifaddrs.h>')
    conf_data.set10('HAVE_GETIFADDRS', 1)
endif

if compiler.has_function('getpwnam', prefix : '#include <pwd.h>')
    conf_data.set10('HAVE_GETPWNAM', 1)
endif

if compiler.has_function('inet_pton', prefix : '#include <arpa/inet.h>')
    conf_data.set10('HAVE_INET_PTON', 1)
endif

if compiler.has_function('inet_ntop', prefix : '#include <arpa/inet.h>')
    conf_data.set10('HAVE_INET_NTOP', 1)
endif

if compiler.has_function('fflush', prefix : '#include <stdio.h>')
    conf_data.set10('HAVE_FFLUSH', 1)
endif

if compiler.has_function('fsync', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_FSYNC', 1)
endif

if compiler.has_function('fseeko', prefix : '#include <stdio.h>')
    conf_data.set10('HAVE_FSEEKO', 1)
endif

if compiler.has_function('ftello', prefix : '#include <stdio.h>')
    conf_data.set10('HAVE_FTELLO', 1)
endif

if compiler.has_function('sigaction', prefix : '#include <signal.h>')
    conf_data.set10('HAVE_SIGACTION', 1)
endif

if compiler.has_function('gettimeofday', prefix : '#include <sys/time.h>')
    conf_data.set10('HAVE_GETTIMEOFDAY', 1)
endif

if compiler.has_function('clock_gettime', prefix : '#include <time.h>')
    conf_data.set10('HAVE_CLOCK_GETTIME', 1)
endif

if compiler.has_function('setitimer', prefix : '#include <sys/time.h>')
    conf_data.set10('HAVE_SETITIMER', 1)
endif

if compiler.has_function('setenv', prefix : '#include <stdlib.h>')
    conf_data.set10('HAVE_SETENV', 1)
    conf_data.set('HAVE_SETENV_OR_PUTENV', true)
endif

if compiler.has_function('putenv', prefix : '#include <stdlib.h>')
    conf_data.set10('HAVE_PUTENV', 1)
    conf_data.set('HAVE_SETENV_OR_PUTENV', true)
endif

if compiler.has_function('unsetenv', prefix : '#include <stdlib.h>')
    conf_data.set10('HAVE_UNSETENV', 1)
endif

if compiler.has_function('getuid', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_GETUID', 1)
endif

if compiler.has_function('geteuid', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_GETEUID', 1)
endif

if compiler.has_function('wcwidth', prefix : '#include <wchar.h>', args: '-D_XOPEN_SOURCE')
    conf_data.set10('HAVE_WCWIDTH', 1)
endif

if compiler.has_function('fork', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_FORK', 1)
endif

if compiler.has_function('getpid', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_GETPID', 1)
endif

if compiler.has_function('setpgid', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_SETPGID', 1)
endif

if compiler.has_function('getpgid', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_GETPGID', 1)
endif

if compiler.has_function('setpgrp', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_SETPGRP', 1)
endif

if compiler.has_function('getpgrp', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_GETPGRP', 1)
endif

if compiler.has_function('raise', prefix : '#include <signal.h>')
    conf_data.set10('HAVE_RAISE', 1)
endif

if compiler.has_function('kill', prefix : '#include <signal.h>')
    conf_data.set10('HAVE_KILL', 1)
endif

if compiler.has_function('fpathconf', prefix : '#include <unistd.h>')
    conf_data.set10('HAVE_FPATHCONF', 1)
endif

if compiler.has_function('poll', prefix : '#include <poll.h>')
    conf_data.set10('HAVE_POLL', 1)
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

if compiler.has_function('RAND_bytes', prefix: '#include <openssl/rand.h>', dependencies: ssldeps)
    conf_data.set('HAVE_RAND_BYTES', 1)
endif

if compiler.has_function('ASN1_STRING_get0_data', prefix: '#include <openssl/asn1.h>', dependencies: ssldeps)
    conf_data.set('HAVE_ASN1_STRING_GET0_DATA', 1)
endif

if compiler.has_function('nl_langinfo', prefix: '#include <langinfo.h>')
    conf_data.set('HAVE_LANGINFO_CODESET', 1)
endif

if compiler.has_function('iconv', prefix: '#include <iconv.h>')
    conf_data.set('HAVE_ICONV', 1)
endif

if compiler.has_function('alloca', prefix: '#include <alloca.h>')
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

conf_data.set('ICONV_CONST', true)

sysconfdir = get_option('prefix') / get_option('sysconfdir')/'elinks'
conf_data.set('CONFDIR', sysconfdir)
conf_data.set('sysconfdir', sysconfdir)
conf_data.set('SOMETHING', '@SOMETHING@')
conf_data.set('api_srcdir', srcdir + '/src')
conf_data.set('LIBDIR', get_option('prefix') / 'lib')
conf_data.set('LOCALEDIR', get_option('prefix') / 'share/locale')

conf_data.set('HAVE_SA_STORAGE', true)

conf_data.set('SIZEOF_CHAR', compiler.sizeof('char'))
conf_data.set('SIZEOF_SHORT', compiler.sizeof('short'))
conf_data.set('SIZEOF_INT', compiler.sizeof('int'))
conf_data.set('SIZEOF_LONG', compiler.sizeof('long'))
conf_data.set('SIZEOF_LONG_LONG', compiler.sizeof('long long'))
conf_data.set('SIZEOF_OFF_T', compiler.sizeof('off_t'))

conf_data.set('CONFIG_GNUTLS_OPENSSL_COMPAT', false)

configure_file(input : 'config2.h.in',
               output : 'config.h',
               configuration : conf_data)

subdir('po')
subdir('src')
subdir('contrib')
subdir('doc')
