# vi: ft=python

import sys, os, re
sys.path.insert(0, os.path.join(os.getcwd(), 'waftools'))
sys.path.insert(0, os.getcwd())
from shlex import split
from waflib.Configure import conf
from waflib.Tools import c_preproc
from waflib import Utils
from waftools.checks.generic import *
from waftools.checks.custom import *

c_preproc.go_absolute=True # enable system folders
c_preproc.standard_includes.append('/usr/local/include')

APPNAME = 'mpa'

"""
Dependency identifiers (for win32 vs. Unix):
    wscript / C source                  meaning
    --------------------------------------------------------------------------
    posix / HAVE_POSIX:                 defined on Linux, OSX, Cygwin
                                        (Cygwin emulates POSIX APIs on Windows)
    mingw / __MINGW32__:                defined if posix is not defined
                                        (Windows without Cygwin)
    os-win32 / _WIN32:                  defined if basic windows.h API is available
    win32-desktop / HAVE_WIN32_DESKTOP: defined if desktop windows.h API is available
    uwp / HAVE_UWP:                     defined if building for UWP (basic Windows only)
"""

build_options = [
    {
        'name': 'libaf',
        'desc': 'internal audio filter chain',
        'default': 'enable',
        'func': check_true,
    }, {
        'name': '--cplayer',
        'desc': 'mpa CLI player',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--libmpa-shared',
        'desc': 'shared library',
        'default': 'disable',
        'func': check_true
    }, {
        'name': '--libmpa-static',
        'desc': 'static library',
        'default': 'disable',
        'deps': '!libmpa-shared',
        'func': check_true
    }, {
        'name': '--static-build',
        'desc': 'static build',
        'default': 'disable',
        'func': check_true
    }, {
        'name': '--build-date',
        'desc': 'whether to include binary compile time',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--optimize',
        'desc': 'whether to optimize',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--debug-build',
        'desc': 'whether to compile-in debugging information',
        'default': 'enable',
        'func': check_true
    }, {
        'name': 'libdl',
        'desc': 'dynamic loader',
        'func': check_libs(['dl'], check_statement('dlfcn.h', 'dlopen("", 0)'))
    }, {
        'name': '--cplugins',
        'desc': 'C plugins',
        'deps': 'libdl && !os-win32',
        'func': check_cc(linkflags=['-rdynamic']),
    }, {
        'name': '--zsh-comp',
        'desc': 'zsh completion',
        'func': check_ctx_vars('BIN_PERL'),
        'func': check_true,
        'default': 'disable',
    }, {
        # does nothing - left for backward and forward compatibility
        'name': '--asm',
        'desc': 'inline assembly (currently without effect)',
        'default': 'enable',
        'func': check_true,
    }, {
        'name': '--test',
        'desc': 'test suite (using cmocka)',
        'func': check_pkg_config('cmocka', '>= 1.0.0'),
        'default': 'disable',
    }, {
        'name': '--clang-database',
        'desc': 'generate a clang compilation database',
        'func': check_true,
        'default': 'disable',
    }
]

main_dependencies = [
    {
        'name': 'noexecstack',
        'desc': 'compiler support for noexecstack',
        'func': check_cc(linkflags='-Wl,-z,noexecstack')
    }, {
        'name': 'noexecstack',
        'desc': 'linker support for --nxcompat --no-seh --dynamicbase',
        'func': check_cc(linkflags=['-Wl,--nxcompat', '-Wl,--no-seh', '-Wl,--dynamicbase'])
    } , {
        'name': 'libm',
        'desc': '-lm',
        'func': check_cc(lib='m')
    }, {
        'name': 'mingw',
        'desc': 'MinGW',
        'deps': 'os-win32',
        'func': check_statement('stdlib.h', 'int x = __MINGW32__;'
                                            'int y = __MINGW64_VERSION_MAJOR'),
    }, {
        'name': 'posix',
        'desc': 'POSIX environment',
        # This should be good enough.
        'func': check_statement(['poll.h', 'unistd.h', 'sys/mman.h'],
            'struct pollfd pfd; poll(&pfd, 1, 0); fork(); int f[2]; pipe(f); munmap(f,0)'),
    }, {
        'name': '--android',
        'desc': 'Android environment',
        'func': check_statement('android/api-level.h', '(void)__ANDROID__'),  # arbitrary android-specific header
    }, {
        'name': 'posix-or-mingw',
        'desc': 'development environment',
        'deps': 'posix || mingw',
        'func': check_true,
        'req': True,
        'fmsg': 'Unable to find either POSIX or MinGW-w64 environment, ' \
                'or compiler does not work.',
    }, {
        'name': '--uwp',
        'desc': 'Universal Windows Platform',
        'default': 'disable',
        'deps': 'os-win32 && mingw && !cplayer',
        'func': check_cc(lib=['windowsapp']),
    }, {
        'name': 'win32-desktop',
        'desc': 'win32 desktop APIs',
        'deps': '(os-win32 || os-cygwin) && !uwp',
        'func': check_cc(lib=['winmm', 'gdi32', 'ole32', 'uuid', 'avrt', 'dwmapi', 'version']),
    }, {
        'name': '--win32-internal-pthreads',
        'desc': 'internal pthread wrapper for win32 (Vista+)',
        'deps': 'os-win32 && !posix',
        'func': check_true,
    }, {
        'name': 'pthreads',
        'desc': 'POSIX threads',
        'func': check_pthreads,
        'req': True,
        'fmsg': 'Unable to find pthreads support.'
    }, {
        'name': 'gnuc',
        'desc': 'GNU C extensions',
        'func': check_statement([], "__GNUC__"),
    }, {
        'name': 'stdatomic',
        'desc': 'stdatomic.h',
        'func': check_libs(['atomic'],
            check_statement('stdatomic.h',
                'atomic_int_least64_t test = ATOMIC_VAR_INIT(123);'
                'atomic_fetch_add(&test, 1)'))
    }, {
        'name': 'atomics',
        'desc': 'stdatomic.h support or slow emulation',
        'func': check_true,
        'req': True,
        'deps': 'stdatomic || gnuc',
    }, {
        'name': 'librt',
        'desc': 'linking with -lrt',
        'deps': 'pthreads',
        'func': check_cc(lib='rt')
    }, {
        'name': 'dos-paths',
        'desc': 'w32/dos paths',
        'deps': 'os-win32 || os-cygwin',
        'func': check_true
    }, {
        'name': 'posix-spawn-native',
        'desc': 'spawnp()/kill() POSIX support',
        'func': check_statement(['spawn.h', 'signal.h'],
            'posix_spawnp(0,0,0,0,0,0); kill(0,0)'),
        'deps': '!mingw',
    }, {
        'name': 'posix-spawn-android',
        'desc': 'spawnp()/kill() Android replacement',
        'func': check_true,
        'deps': 'android && !posix-spawn-native',
    },{
        'name': 'posix-spawn',
        'desc': 'any spawnp()/kill() support',
        'deps': 'posix-spawn-native || posix-spawn-android',
        'func': check_true,
    }, {
        'name': 'win32-pipes',
        'desc': 'Windows pipe support',
        'func': check_true,
        'deps': 'win32-desktop && !posix',
    }, {
        'name': 'glob-posix',
        'desc': 'glob() POSIX support',
        'deps': '!(os-win32 || os-cygwin)',
        'func': check_statement('glob.h', 'glob("filename", 0, 0, 0)'),
    }, {
        'name': 'glob-win32',
        'desc': 'glob() win32 replacement',
        'deps': '!posix && (os-win32 || os-cygwin)',
        'func': check_true
    }, {
        'name': 'glob',
        'desc': 'any glob() support',
        'deps': 'glob-posix || glob-win32',
        'func': check_true,
    }, {
        'name': 'fchmod',
        'desc': 'fchmod()',
        'func': check_statement('sys/stat.h', 'fchmod(0, 0)'),
    }, {
        'name': 'vt.h',
        'desc': 'vt.h',
        'func': check_statement(['sys/vt.h', 'sys/ioctl.h'],
                                'int m; ioctl(0, VT_GETMODE, &m)'),
    }, {
        'name': 'gbm.h',
        'desc': 'gbm.h',
        'func': check_cc(header_name=['stdio.h', 'gbm.h']),
    }, {
        'name': 'glibc-thread-name',
        'desc': 'GLIBC API for setting thread name',
        'func': check_statement('pthread.h',
                                'pthread_setname_np(pthread_self(), "ducks")',
                                use=['pthreads']),
    }, {
        'name': 'osx-thread-name',
        'desc': 'OSX API for setting thread name',
        'deps': '!glibc-thread-name',
        'func': check_statement('pthread.h',
                                'pthread_setname_np("ducks")', use=['pthreads']),
    }, {
        'name': 'bsd-thread-name',
        'desc': 'BSD API for setting thread name',
        'deps': '!(glibc-thread-name || osx-thread-name)',
        'func': check_statement('pthread.h',
                                'pthread_set_name_np(pthread_self(), "ducks")',
                                use=['pthreads']),
    }, {
        'name': 'bsd-fstatfs',
        'desc': "BSD's fstatfs()",
        'func': check_statement(['sys/param.h', 'sys/mount.h'],
                                'struct statfs fs; fstatfs(0, &fs); fs.f_fstypename')
    }, {
        'name': 'linux-fstatfs',
        'desc': "Linux's fstatfs()",
        'deps': 'os-linux',
        'func': check_statement('sys/vfs.h',
                                'struct statfs fs; fstatfs(0, &fs); fs.f_namelen')
    } , {
        'name': '--zlib',
        'desc': 'zlib',
        'func': check_libs(['z'],
                    check_statement('zlib.h', 'inflate(0, Z_NO_FLUSH)')),
        'req': True,
        'fmsg': 'Unable to find development files for zlib.'
    }, {
        'name': '--rubberband',
        'desc': 'librubberband support',
        'deps': 'libaf',
        'func': check_pkg_config('rubberband', '>= 1.8.0'),
    }
]

ffmpeg_pkg_config_checks = [
    'libavutil',     '>= 56.12.100',
    'libavcodec',    '>= 58.16.100',
    'libavformat',   '>= 58.9.100',
    'libavfilter',   '>= 7.14.100',
    'libswresample', '>= 3.0.100',
]
libav_pkg_config_checks = [
    'libavutil',     '>= 56.6.0',
    'libavcodec',    '>= 58.8.0',
    'libavformat',   '>= 58.1.0',
    'libavfilter',   '>= 7.0.0',
    'libavresample', '>= 4.0.0',
]

def check_ffmpeg_or_libav_versions():
    def fn(ctx, dependency_identifier, **kw):
        versions = ffmpeg_pkg_config_checks
        if ctx.dependency_satisfied('libav'):
            versions = libav_pkg_config_checks
        return check_pkg_config(*versions)(ctx, dependency_identifier, **kw)
    return fn

libav_dependencies = [
    {
        'name': 'libavcodec',
        'desc': 'FFmpeg/Libav present',
        'func': check_pkg_config('libavcodec'),
        'req': True,
        'fmsg': "FFmpeg/Libav development files not found.",
    }, {
        'name': 'ffmpeg',
        'desc': 'libav* is FFmpeg',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? 1 : -1]',
                                use='libavcodec'),
    }, {
        # This check should always result in the opposite of ffmpeg-*.
        # Run it to make sure is_ffmpeg didn't fail for some other reason than
        # the actual version check.
        'name': 'libav',
        'desc': 'libav* is Libav',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? -1 : 1]',
                                use='libavcodec')
    }, {
        'name': 'libav-any',
        'desc': 'Libav/FFmpeg library versions',
        'deps': 'ffmpeg || libav',
        'func': check_ffmpeg_or_libav_versions(),
        'req': True,
        'fmsg': "Unable to find development files for some of the required \
FFmpeg/Libav libraries. Git master is recommended."
    }
]

audio_output_features = [
    {
        'name': '--sdl2',
        'desc': 'SDL2',
        'func': check_pkg_config('sdl2'),
        'default': 'disable'
    }, {
        'name': '--rsound',
        'desc': 'RSound audio output',
        'func': check_statement('rsound.h', 'rsd_init(NULL)', lib='rsound')
    }, {
        'name': '--sndio',
        'desc': 'sndio audio input/output',
        'func': check_statement('sndio.h',
            'struct sio_par par; sio_initpar(&par); const char *s = SIO_DEVANY', lib='sndio'),
        'default': 'disable'
    }, {
        'name': '--pulse',
        'desc': 'PulseAudio audio output',
        'func': check_pkg_config('libpulse', '>= 1.0')
    }, {
        'name': '--openal',
        'desc': 'OpenAL audio output',
        'func': check_pkg_config('openal', '>= 1.13'),
        'default': 'disable'
    }, {
        'name': '--opensles',
        'desc': 'OpenSL ES audio output',
        'func': check_statement('SLES/OpenSLES.h', 'slCreateEngine', lib="OpenSLES"),
    }, {
        'name': '--alsa',
        'desc': 'ALSA audio output',
        'func': check_pkg_config('alsa', '>= 1.0.18'),
    }, {
        'name': '--coreaudio',
        'desc': 'CoreAudio audio output',
        'func': check_cc(
            fragment=load_fragment('coreaudio.c'),
            framework_name=['CoreFoundation', 'CoreAudio', 'AudioUnit', 'AudioToolbox'])
    }, {
        'name': '--audiounit',
        'desc': 'AudioUnit output for iOS',
        'deps': 'atomics',
        'func': check_cc(
            fragment=load_fragment('audiounit.c'),
            framework_name=['Foundation', 'AudioToolbox'])
    }, {
        'name': '--wasapi',
        'desc': 'WASAPI audio output',
        'deps': 'os-win32 || os-cygwin',
        'func': check_cc(fragment=load_fragment('wasapi.c')),
    }
]

standalone_features = [
    {
        'name': 'win32-executable',
        'desc': 'w32 executable',
        'deps': 'os-win32 || !(!(os-cygwin))',
        'func': check_ctx_vars('WINDRES')
    }
]

_INSTALL_DIRS_LIST = [
    ('confdir', '${SYSCONFDIR}/mpa',  'configuration files'),
    ('zshdir',  '${DATADIR}/zsh/site-functions', 'zsh completion functions'),
    ('confloaddir', '${CONFDIR}', 'configuration files load directory'),
]

def options(opt):
    opt.load('compiler_c')
    opt.load('waf_customizations')
    opt.load('features')
    opt.load('gnu_dirs')

    #remove unused options from gnu_dirs
    opt.parser.remove_option("--sbindir")
    opt.parser.remove_option("--libexecdir")
    opt.parser.remove_option("--sharedstatedir")
    opt.parser.remove_option("--localstatedir")
    opt.parser.remove_option("--oldincludedir")
    opt.parser.remove_option("--infodir")
    opt.parser.remove_option("--localedir")
    opt.parser.remove_option("--dvidir")
    opt.parser.remove_option("--pdfdir")
    opt.parser.remove_option("--psdir")

    libdir = opt.parser.get_option('--libdir')
    if libdir:
        # Replace any mention of lib64 as we keep the default
        # for libdir the same as before the waf update.
        libdir.help = libdir.help.replace('lib64', 'lib')

    group = opt.get_option_group("Installation directories")
    for ident, default, desc in _INSTALL_DIRS_LIST:
        group.add_option('--{0}'.format(ident),
            type    = 'string',
            dest    = ident,
            default = default,
            help    = 'directory for installing {0} [{1}]' \
                      .format(desc, default.replace('${','').replace('}','')))

    group = opt.get_option_group("build and install options")
    group.add_option('--variant',
        default = '',
        help    = 'variant name for saving configuration and build results')

    opt.parse_features('build and install options', build_options)
    optional_features = main_dependencies + libav_dependencies
    opt.parse_features('optional features', optional_features)
    opt.parse_features('audio outputs',     audio_output_features)
    opt.parse_features('standalone app',    standalone_features)

@conf
def is_optimization(ctx):
    return getattr(ctx.options, 'enable_optimize')

@conf
def is_debug_build(ctx):
    return getattr(ctx.options, 'enable_debug-build')

def configure(ctx):
    from waflib import Options
    ctx.resetenv(ctx.options.variant)
    ctx.check_waf_version(mini='1.8.4')
    target = os.environ.get('TARGET')
    (cc, pkg_config, ar, windres) = ('cc', 'pkg-config', 'ar', 'windres')

    if target:
        cc         = '-'.join([target, 'gcc'])
        pkg_config = '-'.join([target, pkg_config])
        ar         = '-'.join([target, ar])
        windres    = '-'.join([target, windres])

    ctx.find_program(cc,          var='CC')
    ctx.find_program(pkg_config,  var='PKG_CONFIG')
    ctx.find_program(ar,          var='AR')
    ctx.find_program(windres,     var='WINDRES',   mandatory=False)
    ctx.find_program('perl',      var='BIN_PERL',  mandatory=False)

    ctx.add_os_flags('LIBRARY_PATH')

    ctx.load('compiler_c')
    ctx.load('waf_customizations')
    ctx.load('dependencies')
    ctx.load('detections.compiler')
    ctx.load('gnu_dirs')

    # if libdir is not set in command line options,
    # override the gnu_dirs default in order to
    # always have `lib/` as the library directory.
    if not getattr(Options.options, 'LIBDIR', None):
        ctx.env['LIBDIR'] = Utils.subst_vars(os.path.join('${EXEC_PREFIX}', 'lib'), ctx.env)

    for ident, _, _ in _INSTALL_DIRS_LIST:
        varname = ident.upper()
        ctx.env[varname] = getattr(ctx.options, ident)

        # keep substituting vars, until the paths are fully expanded
        while re.match('\$\{([^}]+)\}', ctx.env[varname]):
            ctx.env[varname] = Utils.subst_vars(ctx.env[varname], ctx.env)

    ctx.parse_dependencies(build_options)
    ctx.parse_dependencies(main_dependencies)
    ctx.parse_dependencies(audio_output_features)
    ctx.parse_dependencies(libav_dependencies)

    ctx.parse_dependencies(standalone_features)

    ctx.load('generators.headers')

    if not ctx.dependency_satisfied('build-date'):
        ctx.env.CFLAGS += ['-DNO_BUILD_TIMESTAMPS']

    if ctx.dependency_satisfied('clang-database'):
        ctx.load('clang_compilation_database')

    if ctx.dependency_satisfied('cplugins'):
        # We need to export the libmpa symbols, since the mpa binary itself is
        # not linked against libmpa. The C plugin needs to be able to pick
        # up the libmpa symbols from the binary. We still restrict the set
        # of exported symbols via mpa.def.
        ctx.env.LINKFLAGS += ['-rdynamic']

    ctx.store_dependencies_lists()

def __write_version__(ctx):
    ctx.env.VERSIONH_ST = '--versionh="%s"'
    ctx.env.CWD_ST = '--cwd="%s"'
    ctx.env.VERSIONSH_CWD = [ctx.srcnode.abspath()]

    ctx(
        source = 'version.sh',
        target = 'version.h',
        rule   = 'sh ${SRC} ${CWD_ST:VERSIONSH_CWD} ${VERSIONH_ST:TGT}',
        always = True,
        update_outputs = True)

def build(ctx):
    if ctx.options.variant not in ctx.all_envs:
        from waflib import Errors
        raise Errors.WafError(
            'The project was not configured: run "waf --variant={0} configure" first!'
                .format(ctx.options.variant))
    ctx.unpack_dependencies_lists()
    ctx.add_group('versionh')
    ctx.add_group('sources')

    ctx.set_group('versionh')
    __write_version__(ctx)
    ctx.set_group('sources')
    ctx.load('wscript_build')

def init(ctx):
    from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext
    for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
        class tmp(y):
            variant = ctx.options.variant

    # This is needed because waf initializes the ConfigurationContext with
    # an arbitrary setenv('') which would rewrite the previous configuration
    # cache for the default variant if the configure step finishes.
    # Ideally ConfigurationContext should just let us override this at class
    # level like the other Context subclasses do with variant
    from waflib.Configure import ConfigurationContext
    class cctx(ConfigurationContext):
        def resetenv(self, name):
            self.all_envs = {}
            self.setenv(name)
