const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
    });
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addIncludePath(b.path(".."));
    mod.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    mod.addCSourceFiles(.{
        .root = b.path(".."),
        .files = &.{
            "libopna/opnaadpcm.c",
            "libopna/opnadrum.c",
            "libopna/opnafm.c",
            "libopna/opnassg.c",
            "libopna/opnassg-sinc-c.c",
            "libopna/opnatimer.c",
            "libopna/opna.c",
            "fmdriver/fmdriver_fmp.c",
            "fmdriver/fmdriver_pmd.c",
            "fmdriver/fmdriver_common.c",
            "fmdriver/ppz8.c",
            "fft/fft.c",
            "fmdsp/fmdsp-pacc.c",
            "fmdsp/font_fmdsp_small.c",
            "fmdsp/font_rom.c",
            "web/main.c",
            "web/pacc-webgl.c",
        },
        .flags = &.{
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-Wno-unknown-attributes", // due to optimize attribute
            "-std=c11",
        },
    });

    const exe = b.addExecutable(.{
        .name = "main",
        .root_module = mod,
    });
    exe.entry = .disabled;
    exe.wasi_exec_model = .reactor;
    exe.rdynamic = true;

    const static_files: []const []const u8 = &.{
        "index.html",
        "index.js",
        "pacc-webgl.js",
    };
    for (static_files) |static_file| {
        b.getInstallStep().dependOn(&b.addInstallFileWithDir(b.path(static_file), .prefix, static_file).step);
    }
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(exe.getEmittedBin(), .prefix, "main.wasm").step);
}
