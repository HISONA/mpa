import re
import os

def _all_includes(ctx):
    return [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
            ctx.dependencies_includes()

def build(ctx):
    ctx.load('waf_customizations')
    ctx.load('generators.sources')

    ctx(
        features = "file2string",
        source = "etc/input.conf",
        target = "input/input_conf.h",
    )

    ctx(
        features = "file2string",
        source = "etc/builtin.conf",
        target = "player/builtin_conf.inc",
    )

    if ctx.dependency_satisfied('cplayer'):
        main_fn_c = ctx.pick_first_matching_dep([
            ( "osdep/main-fn-unix.c",                "posix" ),
            ( "osdep/main-fn-win.c",                 "win32-desktop" ),
        ])

    getch2_c = ctx.pick_first_matching_dep([
        ( "osdep/terminal-unix.c",               "posix" ),
        ( "osdep/terminal-win.c",                "win32-desktop" ),
        ( "osdep/terminal-dummy.c" ),
    ])

    timer_c = ctx.pick_first_matching_dep([
        ( "osdep/timer-win2.c",                  "os-win32" ),
        ( "osdep/timer-darwin.c",                "os-darwin" ),
        ( "osdep/timer-linux.c",                 "posix" ),
    ])

    ipc_c = ctx.pick_first_matching_dep([
        ( "input/ipc-unix.c",                    "posix" ),
        ( "input/ipc-win.c",                     "win32-desktop" ),
        ( "input/ipc-dummy.c" ),
    ])

    subprocess_c = ctx.pick_first_matching_dep([
        ( "osdep/subprocess-posix.c",            "posix-spawn" ),
        ( "osdep/subprocess-win.c",              "win32-desktop" ),
        ( "osdep/subprocess-dummy.c" ),
    ])

    sources = [
        ## Audio
        ( "audio/aframe.c" ),
        ( "audio/audio_buffer.c" ),
        ( "audio/chmap.c" ),
        ( "audio/chmap_sel.c" ),
        ( "audio/decode/ad_lavc.c" ),
        ( "audio/decode/ad_spdif.c" ),
        ( "audio/filter/af_format.c" ),
        ( "audio/filter/af_lavrresample.c" ),
        ( "audio/filter/af_rubberband.c",        "rubberband" ),
        ( "audio/filter/af_scaletempo.c" ),
        ( "audio/fmt-conversion.c" ),
        ( "audio/format.c" ),
        ( "audio/out/ao.c" ),
        ( "audio/out/ao_alsa.c",                 "alsa" ),
        ( "audio/out/ao_audiounit.m",            "audiounit" ),
        ( "audio/out/ao_coreaudio.c",            "coreaudio" ),
        ( "audio/out/ao_coreaudio_chmap.c",      "coreaudio || audiounit" ),
        ( "audio/out/ao_coreaudio_exclusive.c",  "coreaudio" ),
        ( "audio/out/ao_coreaudio_properties.c", "coreaudio" ),
        ( "audio/out/ao_coreaudio_utils.c",      "coreaudio || audiounit" ),
        ( "audio/out/ao_null.c" ),
        ( "audio/out/ao_openal.c",               "openal" ),
        ( "audio/out/ao_opensles.c",             "opensles" ),
        ( "audio/out/ao_pulse.c",                "pulse" ),
        ( "audio/out/ao_rsound.c",               "rsound" ),
        ( "audio/out/ao_sdl.c",                  "sdl2" ),
        ( "audio/out/ao_sndio.c",                "sndio" ),
        ( "audio/out/ao_wasapi.c",               "wasapi" ),
        ( "audio/out/ao_wasapi_changenotify.c",  "wasapi" ),
        ( "audio/out/ao_wasapi_utils.c",         "wasapi" ),
        ( "audio/out/pull.c" ),
        ( "audio/out/push.c" ),

        ## Core
        ( "common/av_common.c" ),
        ( "common/av_log.c" ),
        ( "common/codecs.c" ),
        ( "common/common.c" ),
        ( "common/msg.c" ),
        ( "common/playlist.c" ),
        ( "common/tags.c" ),
        ( "common/version.c" ),

        ## Demuxers
        ( "demux/codec_tags.c" ),
        ( "demux/cue.c" ),
        ( "demux/demux.c" ),
        ( "demux/demux_cue.c" ),
        ( "demux/demux_lavf.c" ),
        ( "demux/demux_null.c" ),
        ( "demux/demux_playlist.c" ),
        ( "demux/demux_raw.c" ),
        ( "demux/demux_timeline.c" ),
        ( "demux/packet.c" ),
        ( "demux/timeline.c" ),

        ( "filters/f_auto_filters.c" ),
        ( "filters/f_autoconvert.c" ),
        ( "filters/f_decoder_wrapper.c" ),
        ( "filters/f_demux_in.c" ),
        ( "filters/f_lavfi.c" ),
        ( "filters/f_output_chain.c" ),
        ( "filters/f_swresample.c" ),
        ( "filters/f_utils.c" ),
        ( "filters/filter.c" ),
        ( "filters/frame.c" ),
        ( "filters/user_filters.c" ),

        ## Input
        ( "input/cmd.c" ),
        ( "input/input.c" ),
        ( "input/ipc.c" ),
        ( ipc_c ),
        ( "input/keycodes.c" ),
        ( "input/pipe-win32.c",                  "win32-pipes" ),

        ## Misc
        ( "misc/bstr.c" ),
        ( "misc/charset_conv.c" ),
        ( "misc/dispatch.c" ),
        ( "misc/json.c" ),
        ( "misc/node.c" ),
        ( "misc/rendezvous.c" ),
        ( "misc/ring.c" ),
        ( "misc/thread_pool.c" ),
        ( "misc/thread_tools.c" ),

        ## Options
        ( "options/m_config.c" ),
        ( "options/m_option.c" ),
        ( "options/m_property.c" ),
        ( "options/options.c" ),
        ( "options/parse_commandline.c" ),
        ( "options/parse_configfile.c" ),
        ( "options/path.c" ),

        ## Player
        ( "player/audio.c" ),
        ( "player/client.c" ),
        ( "player/command.c" ),
        ( "player/configfiles.c" ),
        ( "player/loadfile.c" ),
        ( "player/main.c" ),
        ( "player/misc.c" ),
        ( "player/osd.c" ),
        ( "player/playloop.c" ),

        ## Streams
        ( "stream/cookies.c" ),
        ( "stream/stream.c" ),
        ( "stream/stream_cb.c" ),
        ( "stream/stream_file.c" ),
        ( "stream/stream_lavf.c" ),
        ( "stream/stream_memory.c" ),
        ( "stream/stream_null.c" ),

        ## osdep
        ( getch2_c ),
        ( "osdep/io.c" ),
        ( "osdep/threads.c" ),
        ( "osdep/timer.c" ),
        ( timer_c ),
        ( "osdep/polldev.c",                     "posix" ),

        ( "osdep/android/posix-spawn.c",         "android"),
        ( "osdep/android/strnlen.c",             "android"),
        ( "osdep/glob-win.c",                    "glob-win32" ),
        ( "osdep/mpa.rc",                        "win32-executable" ),
        ( "osdep/path-unix.c"),
        ( "osdep/path-uwp.c",                    "uwp" ),
        ( "osdep/path-win.c",                    "win32-desktop" ),
        ( "osdep/semaphore_osx.c" ),
        ( "osdep/subprocess.c" ),
        ( subprocess_c ),
        ( "osdep/w32_keyboard.c",                "os-cygwin" ),
        ( "osdep/w32_keyboard.c",                "os-win32" ),
        ( "osdep/win32/pthread.c",               "win32-internal-pthreads"),
        ( "osdep/windows_utils.c",               "os-cygwin" ),
        ( "osdep/windows_utils.c",               "os-win32" ),

        ## tree_allocator
        "ta/ta.c", "ta/ta_talloc.c", "ta/ta_utils.c"
    ]

    if ctx.dependency_satisfied('win32-executable'):
        from waflib import TaskGen

        TaskGen.declare_chain(
            name    = 'windres',
            rule    = '${WINDRES} ${WINDRES_FLAGS} ${SRC} ${TGT}',
            ext_in  = '.rc',
            ext_out = '-rc.o',
            color   = 'PINK')

        ctx.env.WINDRES_FLAGS = [
            '--include-dir={0}'.format(ctx.bldnode.abspath()),
            '--include-dir={0}'.format(ctx.srcnode.abspath()),
            '--codepage=65001' # Unicode codepage
        ]

        for node in 'osdep/mpa.exe.manifest etc/mpa-icon.ico'.split():
            ctx.add_manual_dependency(
                ctx.path.find_node('osdep/mpa.rc'),
                ctx.path.find_node(node))

        version = ctx.bldnode.find_node('version.h')
        if version:
            ctx.add_manual_dependency(
                ctx.path.find_node('osdep/mpa.rc'),
                version)

    if ctx.dependency_satisfied('cplayer') or ctx.dependency_satisfied('test'):
        ctx(
            target       = "objects",
            source       = ctx.filtered_sources(sources),
            use          = ctx.dependencies_use(),
            includes     = _all_includes(ctx),
            features     = "c",
        )

    syms = False
    if ctx.dependency_satisfied('cplugins'):
        syms = True
        ctx.load("syms")

    if ctx.dependency_satisfied('cplayer'):
        ctx(
            target       = "mpa",
            source       = main_fn_c,
            use          = ctx.dependencies_use() + ['objects'],
            includes     = _all_includes(ctx),
            features     = "c cprogram" + (" syms" if syms else ""),
            export_symbols_def = "libmpa/mpa.def", # for syms=True
            install_path = ctx.env.BINDIR
        )
        for f in ['mpa.conf', 'input.conf']:
            ctx.install_as(os.path.join(ctx.env.DOCDIR, f),
                           os.path.join('etc/', f))

        if ctx.env.DEST_OS == 'win32':
            wrapctx = ctx(
                target       = "mpa",
                source       = ['osdep/win32-console-wrapper.c'],
                features     = "c cprogram",
                install_path = ctx.env.BINDIR
            )

            wrapctx.env.cprogram_PATTERN = "%s.com"
            wrapflags = ['-municode', '-mconsole']
            wrapctx.env.CFLAGS = ctx.env.CFLAGS + wrapflags
            wrapctx.env.LAST_LINKFLAGS = ctx.env.LAST_LINKFLAGS + wrapflags

    if ctx.dependency_satisfied('test'):
        for test in ctx.path.ant_glob("test/*.c"):
            ctx(
                target       = os.path.splitext(test.srcpath())[0],
                source       = test.srcpath(),
                use          = ctx.dependencies_use() + ['objects'],
                includes     = _all_includes(ctx),
                features     = "c cprogram",
                install_path = None,
            )

    build_shared = ctx.dependency_satisfied('libmpa-shared')
    build_static = ctx.dependency_satisfied('libmpa-static')
    if build_shared or build_static:
        if build_shared:
            waftoolsdir = os.path.join(os.path.dirname(__file__), "waftools")
            ctx.load("syms", tooldir=waftoolsdir)
        vre = '#define MPV_CLIENT_API_VERSION MPV_MAKE_VERSION\((.*), (.*)\)'
        libmpa_header = ctx.path.find_node("libmpa/client.h").read()
        major, minor = re.search(vre, libmpa_header).groups()
        libversion = major + '.' + minor + '.0'

        def _build_libmpa(shared):
            features = "c "
            if shared:
                features += "cshlib syms"
            else:
                features += "cstlib"

            libmpa_kwargs = {
                "target": "mpa",
                "source":   ctx.filtered_sources(sources),
                "use":      ctx.dependencies_use(),
                "add_object": "osdep/macOS_swift.o",
                "includes": [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
                             ctx.dependencies_includes(),
                "features": features,
                "export_symbols_def": "libmpa/mpa.def",
                "install_path": ctx.env.LIBDIR,
                "install_path_implib": ctx.env.LIBDIR,
            }

            if shared and ctx.dependency_satisfied('android'):
                # for Android we just add the linker flag without version
                # as we still need the SONAME for proper linkage.
                # (LINKFLAGS logic taken from waf's apply_vnum in ccroot.py)
                v=ctx.env.SONAME_ST%'libmpa.so'
                ctx.env.append_value('LINKFLAGS',v.split())
            else:
                # for all other configurations we want SONAME to be used
                libmpa_kwargs["vnum"] = libversion

            if shared and ctx.env.DEST_OS == 'win32':
                libmpa_kwargs["install_path"] = ctx.env.BINDIR

            ctx(**libmpa_kwargs)

        if build_shared:
            _build_libmpa(True)
        if build_static:
            _build_libmpa(False)

        def get_deps():
            res = []
            for k in ctx.env.keys():
                if (k.startswith("LIB_") and k != "LIB_ST") \
                or (k.startswith("STLIB_") and k != "STLIB_ST" and k != "STLIB_MARKER"):
                    for l in ctx.env[k]:
                        if l in res:
                            res.remove(l)
                        res.append(l)
            return " ".join(["-l" + l for l in res])

        ctx(
            target       = 'libmpa/mpa.pc',
            source       = 'libmpa/mpa.pc.in',
            features     = 'subst',
            PREFIX       = ctx.env.PREFIX,
            LIBDIR       = ctx.env.LIBDIR,
            INCDIR       = ctx.env.INCLUDEDIR,
            VERSION      = libversion,
            PRIV_LIBS    = get_deps(),
        )

        headers = ["client.h", "qthelper.hpp", "stream_cb.h"]
        for f in headers:
            ctx.install_as(ctx.env.INCLUDEDIR + '/mpa/' + f, 'libmpa/' + f)

        ctx.install_as(ctx.env.LIBDIR + '/pkgconfig/mpa.pc', 'libmpa/mpa.pc')

    if ctx.dependency_satisfied('cplayer'):

        if ctx.dependency_satisfied('zsh-comp'):
            ctx.zshcomp(target = "etc/_mpa", source = "TOOLS/zsh.pl")
            ctx.install_files(
                ctx.env.ZSHDIR,
                ['etc/_mpa'])
