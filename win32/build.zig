const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const cpu = target.result.cpu;
    const enable_neon = cpu.has(.arm, .neon) or cpu.has(.aarch64, .neon);
    const enable_sse = cpu.has(.x86, .sse2);

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.linkSystemLibrary("user32", .{});
    mod.linkSystemLibrary("kernel32", .{});
    mod.linkSystemLibrary("ole32", .{});
    mod.linkSystemLibrary("uuid", .{});
    mod.linkSystemLibrary("comdlg32", .{});
    mod.linkSystemLibrary("gdi32", .{});
    mod.linkSystemLibrary("shlwapi", .{});
    mod.linkSystemLibrary("winmm", .{});
    mod.linkSystemLibrary("shell32", .{});
    mod.linkSystemLibrary("ksuser", .{});
    mod.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    mod.addCMacro("LIBOPNA_ENABLE_OSCILLO", "");
    mod.addCMacro("FMPLAYER_FILE_WIN_UTF16", "");
    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path(".."));
    var files: std.ArrayList([]const u8) = .empty;
    files.appendSlice(b.allocator, &.{
        "win32/main.c",
        "win32/toneview.c",
        "win32/oscilloview.c",
        "win32/wavesave.c",
        "win32/wavewrite.c",
        "win32/soundout.c",
        "win32/dsoundout.c",
        "win32/waveout.c",
        "win32/guid.c",
        "win32/about.c",
        "win32/configdialog.c",
        "common/winfont.c",
        "common/fmplayer_file.c",
        "common/fmplayer_file_win.c",
        "common/fmplayer_drumrom_win.c",
        "common/fmplayer_fontrom_win.c",
        "common/fmplayer_work_opna.c",
        "fft/fft.c",
        "fmdriver/fmdriver_pmd.c",
        "fmdriver/fmdriver_fmp.c",
        "fmdriver/fmdriver_common.c",
        "fmdriver/ppz8.c",
        "fmdsp/fmdsp-pacc.c",
        "fmdsp/fmdsp_platform_win.c",
        "fmdsp/font_fmdsp_small.c",
        "fmdsp/font_rom.c",
        "libopna/opna.c",
        "libopna/opnatimer.c",
        "libopna/opnafm.c",
        "libopna/opnassg.c",
        "libopna/opnassg-sinc-c.c",
        "libopna/opnadrum.c",
        "libopna/opnaadpcm.c",
        "pacc/pacc-d3d9.c",
        "tonedata/tonedata.c",
    }) catch @panic("OOM");
    if (enable_neon) {
        mod.addCMacro("ENABLE_NEON", "");
        files.append(b.allocator, "libopna/opnassg-sinc-neon.s") catch @panic("OOM");
    }
    if (enable_sse) {
        mod.addCMacro("ENABLE_SSE", "");
        files.append(b.allocator, "libopna/opnassg-sinc-sse2.c") catch @panic("OOM");
    }
    mod.addCSourceFiles(.{
        .root = b.path(".."),
        .files = files.items,
        .flags = &.{
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-Wno-cast-function-type-mismatch", // due to proc address cast in pacc-d3d9.c
            "-Wno-unknown-attributes", // due to optimize attribute
            "-Wno-unused-function", // TODO: in fmplayer_file_win.c
            "-fno-sanitize=shift",
        },
    });

    const icotool_run = b.addSystemCommand(&.{ "icotool", "-c" });
    icotool_run.addFileArg(b.path("fmplayer.png"));
    icotool_run.addFileArg(b.path("fmplayer32.png"));
    const icon = icotool_run.addPrefixedOutputFileArg("-o", "fmplayer.ico");
    const manifest = b.addConfigHeader(.{
        .style = .{ .autoconf_at = b.path("lnf.manifest.in") },
        .include_path = "lnf.manifest",
    }, .{
        .VER = "0.14.1", // TODO: read this from build.zig.zon
    });
    mod.addWin32ResourceFile(.{
        .file = b.path("lnf.rc"),
        .include_paths = &.{
            icon.dirname(),
            manifest.getOutputDir(),
        },
    });

    const exe = b.addExecutable(.{
        .name = "98fmplayer",
        .root_module = mod,
    });
    exe.subsystem = .Windows;
    exe.mingw_unicode_entry_point = true;
    b.installArtifact(exe);
}
