const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const cpu = target.result.cpu;
    const enable_neon = cpu.has(.arm, .neon) or cpu.has(.aarch64, .neon);
    const enable_sse = cpu.has(.x86, .sse2);

    const jack = b.option(bool, "jack", "Enable Jack audio backend") orelse true;
    const pulse = b.option(bool, "pulse", "Enable Pulseaudio audio backend") orelse true;
    const alsa = b.option(bool, "alsa", "Enable ALSA audio backend") orelse true;

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.linkSystemLibrary("gtk+-3.0", .{});
    mod.linkSystemLibrary("sndfile", .{});
    mod.linkSystemLibrary("gl", .{});
    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path(".."));
    mod.addIncludePath(b.path("../soundout"));
    mod.addCMacro("_POSIX_C_SOURCE", "200809L");
    mod.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    mod.addCMacro("LIBOPNA_ENABLE_OSCILLO", "");
    mod.addCMacro("PACC_GL_3", "");
    var files: std.ArrayList([]const u8) = .empty;
    files.appendSlice(b.allocator, &.{
        "gtk/main.c",
        "gtk/toneview.c",
        "gtk/wavesave.c",
        "gtk/configdialog.c",
        "gtk/oscilloview-gl.c",
        "common/fmplayer_file.c",
        "common/fmplayer_file_gio.c",
        "common/fmplayer_work_opna.c",
        "common/fmplayer_drumrom_unix.c",
        "common/fmplayer_fontrom_unix.c",
        "fft/fft.c",
        "fmdriver/fmdriver_fmp.c",
        "fmdriver/fmdriver_pmd.c",
        "fmdriver/fmdriver_common.c",
        "fmdriver/ppz8.c",
        "fmdsp/fmdsp-pacc.c",
        "fmdsp/font_rom.c",
        "fmdsp/font_fmdsp_small.c",
        "fmdsp/fmdsp_platform_unix.c",
        "libopna/opnaadpcm.c",
        "libopna/opnadrum.c",
        "libopna/opnafm.c",
        "libopna/opnassg.c",
        "libopna/opnassg-sinc-c.c",
        "libopna/opnatimer.c",
        "libopna/opna.c",
        "pacc/pacc-gl.c",
        "soundout/soundout.c",
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
    if (jack) {
        mod.addCMacro("ENABLE_JACK", "");
        mod.linkSystemLibrary("jack", .{});
        mod.linkSystemLibrary("soxr", .{});
        files.append(b.allocator, "soundout/jackout.c") catch @panic("OOM");
    }
    if (pulse) {
        mod.addCMacro("ENABLE_PULSE", "");
        mod.linkSystemLibrary("libpulse", .{});
        files.append(b.allocator, "soundout/pulseout.c") catch @panic("OOM");
    }
    if (alsa) {
        mod.addCMacro("ENABLE_ALSA", "");
        mod.linkSystemLibrary("alsa", .{});
        files.append(b.allocator, "soundout/alsaout.c") catch @panic("OOM");
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
            "-Wno-deprecated-declarations", // TODO: deprecated GTK API
            "-Wno-unknown-attributes", // due to optimize attribute
            "-Wno-unused-parameter", // TODO: in soundout.c
            "-fno-sanitize=shift",
        },
    });

    const exe = b.addExecutable(.{
        .name = "98fmplayer",
        .root_module = mod,
    });
    b.installArtifact(exe);
}
