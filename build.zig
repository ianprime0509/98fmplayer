const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const cflags: []const []const u8 = &.{
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-std=c11",
        "-Wno-unknown-attributes", // GCC-specific __attribute__((optimize(3))) is used
    };

    const fft = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    fft.addCSourceFiles(.{
        .root = b.path("fft"),
        .files = &.{
            "fft.c",
        },
        .flags = cflags,
    });
    const fft_lib = b.addLibrary(.{
        .name = "fft",
        .root_module = fft,
    });
    fft_lib.installHeadersDirectory(b.path("fft"), "fft", .{});

    const libopna = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    // TODO: figure out how to handle this better
    libopna.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    libopna.addCMacro("LIBOPNA_ENABLE_OSCILLO", "");
    libopna.addIncludePath(b.path(".")); // for leveldata, oscillo
    libopna.addCSourceFiles(.{
        .root = b.path("libopna"),
        .files = &.{
            "opna.c",
            "opnaadpcm.c",
            "opnadrum.c",
            "opnafm.c",
            "opnassg.c",
            "opnassg-sinc-c.c",
            "opnatimer.c",
            "s98gen.c",
        },
        .flags = cflags,
    });
    // TODO: ENABLE_NEON, ENABLE_SSE
    const libopna_lib = b.addLibrary(.{
        .name = "libopna",
        .root_module = libopna,
    });
    libopna_lib.installHeadersDirectory(b.path("libopna"), "libopna", .{});

    const tonedata = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    tonedata.linkLibrary(libopna_lib);
    tonedata.addCSourceFiles(.{
        .root = b.path("tonedata"),
        .files = &.{
            "tonedata.c",
        },
        .flags = cflags,
    });
    const tonedata_lib = b.addLibrary(.{
        .name = "tonedata",
        .root_module = tonedata,
    });
    tonedata_lib.installHeadersDirectory(b.path("tonedata"), "tonedata", .{});

    const fmdriver = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    fmdriver.addIncludePath(b.path(".")); // for leveldata
    fmdriver.addCSourceFiles(.{
        .root = b.path("fmdriver"),
        .files = &.{
            "fmdriver_common.c",
            "fmdriver_fmp.c",
            "fmdriver_pmd.c",
            "ppz8.c",
        },
        .flags = cflags,
    });
    const fmdriver_lib = b.addLibrary(.{
        .name = "fmdriver",
        .root_module = fmdriver,
    });
    fmdriver_lib.installHeadersDirectory(b.path("fmdriver"), "fmdriver", .{});

    const pacc = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    // TODO: pacc-gl-inc.h has issues, see the TODO comment there.
    const pacc_platform_source = switch (target.result.os.tag) {
        .windows => "pacc-d3d9.c",
        else => "pacc-gl.c",
    };
    const pacc_platform_include = switch (target.result.os.tag) {
        .windows => "pacc-win.h",
        else => "pacc-gl.h",
    };
    pacc.addCMacro("PACC_GL_3", ""); // TODO
    pacc.addCSourceFiles(.{
        .root = b.path("pacc"),
        .files = &.{
            pacc_platform_source,
        },
        .flags = cflags,
    });
    const pacc_lib = b.addLibrary(.{
        .name = "pacc",
        .root_module = pacc,
    });
    pacc_lib.installHeadersDirectory(b.path("pacc"), "pacc", .{});

    const fmdsp = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    fmdsp.addIncludePath(b.path(".")); // for leveldata/leveldata.h, transitively from fmdriver
    fmdsp.linkLibrary(fft_lib);
    fmdsp.linkLibrary(fmdriver_lib);
    fmdsp.linkLibrary(pacc_lib);
    // TODO: duplicated from libopna due to Zig limitations
    fmdsp.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    fmdsp.addCMacro("LIBOPNA_ENABLE_OSCILLO", "");
    // TODO: ENABLE_NEON, ENABLE_SSE
    const fmdsp_platform_source = switch (target.result.os.tag) {
        .windows => "fmdsp_platform_win.c",
        .macos => "fmdsp_platform_mach.c",
        else => src: {
            fmdsp.addCMacro("_POSIX_C_SOURCE", "200809L");
            break :src "fmdsp_platform_unix.c";
        },
    };
    fmdsp.addCSourceFiles(.{
        .root = b.path("fmdsp"),
        .files = &.{
            "fmdsp-pacc.c",
            fmdsp_platform_source,
            "font_fmdsp_small.c",
            "font_rom.c",
        },
        .flags = cflags,
    });
    const fmdsp_lib = b.addLibrary(.{
        .name = "fmdsp",
        .root_module = fmdsp,
    });
    fmdsp_lib.installHeadersDirectory(b.path("pacc"), "pacc", .{
        .include_extensions = &.{
            "pacc.h",
            pacc_platform_include,
        },
    });
    fmdsp_lib.installHeadersDirectory(b.path("fmdsp"), "fmdsp", .{});

    // TODO: figure out what to do here
    const soundout = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    soundout.linkSystemLibrary("alsa", .{});
    soundout.addCMacro("_POSIX_C_SOURCE", "200809L");
    soundout.addCMacro("ENABLE_ALSA", "");
    soundout.addCSourceFiles(.{
        .root = b.path("soundout"),
        .files = &.{
            "soundout.c",
            "alsaout.c",
        },
        .flags = cflags,
    });
    const soundout_lib = b.addLibrary(.{
        .name = "soundout",
        .root_module = soundout,
    });
    soundout_lib.installHeadersDirectory(b.path("soundout"), "soundout", .{
        .include_extensions = &.{
            "soundout.h",
        },
    });

    const common = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    common.linkLibrary(libopna_lib);
    common.linkLibrary(fmdriver_lib);
    common.linkSystemLibrary("gio-2.0", .{}); // TODO
    common.addCMacro("_POSIX_C_SOURCE", "200809L"); // TODO
    // TODO: duplicated from libopna due to Zig limitations
    common.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    common.addCMacro("LIBOPNA_ENABLE_OSCILLO", "");
    common.addIncludePath(b.path(".")); // for leveldata, oscillo
    common.addCSourceFiles(.{
        .root = b.path("common"),
        .files = &.{
            "fmplayer_file.c",
            "fmplayer_work_opna.c",
            // TODO: Windows
            "fmplayer_drumrom_unix.c",
            "fmplayer_file_gio.c", // TODO
            "fmplayer_fontrom_unix.c",
        },
        .flags = cflags,
    });
    const common_lib = b.addLibrary(.{
        .name = "common",
        .root_module = common,
    });
    common_lib.installHeadersDirectory(b.path("common"), "common", .{});

    const fmplayer_gtk = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    fmplayer_gtk.linkSystemLibrary("sndfile", .{});
    fmplayer_gtk.linkSystemLibrary("GL", .{});
    fmplayer_gtk.linkSystemLibrary("gtk+-3", .{
        .use_pkg_config = .force,
    });
    fmplayer_gtk.linkLibrary(tonedata_lib);
    fmplayer_gtk.linkLibrary(libopna_lib);
    fmplayer_gtk.linkLibrary(fmdriver_lib);
    fmplayer_gtk.linkLibrary(fmdsp_lib);
    fmplayer_gtk.linkLibrary(soundout_lib);
    fmplayer_gtk.linkLibrary(common_lib);
    fmplayer_gtk.addIncludePath(b.path(".")); // for leveldata, oscillo
    fmplayer_gtk.addCMacro("PACC_GL_3", "");
    fmplayer_gtk.addCMacro("_POSIX_C_SOURCE", "200809L");
    fmplayer_gtk.addCSourceFiles(.{
        .root = b.path("gtk"),
        .files = &.{
            "configdialog.c",
            "main.c",
            // TODO: oscilloview.c doesn't seem to be needed, since OpenGL is always used regardless
            "oscilloview-gl.c",
            "toneview.c",
            "wavesave.c",
        },
        .flags = cflags,
    });
    const fmplayer_gtk_exe = b.addExecutable(.{
        .name = "98fmplayer",
        .root_module = fmplayer_gtk,
    });
    b.installArtifact(fmplayer_gtk_exe);

    // TODO: Windows stuff
}
