const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .cpu_model = .baseline,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            // See https://webassembly.org/features/ for support
            .atomics,
            .bulk_memory,
            .extended_const,
            .multivalue,
            .mutable_globals,
            .nontrapping_fptoint,
            .reference_types,
            .sign_ext,
            .tail_call,
        }),
        .os_tag = .wasi,
    });
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.export_symbol_names = &.{"__stack_pointer"};
    mod.addIncludePath(b.path(".."));
    mod.addCMacro("_POSIX_C_SOURCE", "199309L");
    mod.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    mod.addCSourceFiles(.{
        .root = b.path(".."),
        .files = &.{
            "common/fmplayer_work_opna.c",
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
            "pacc/pacc-js.c",
            "web/main.c",
        },
        .flags = &.{
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-Wno-unknown-attributes", // due to optimize attribute
            "-std=c23",
            "-fno-sanitize=shift",
        },
    });

    const exe = b.addExecutable(.{
        .name = "main",
        .root_module = mod,
    });
    exe.entry = .disabled;
    exe.wasi_exec_model = .reactor;
    exe.import_memory = true;
    exe.initial_memory = 64 * 1024 * 1024;
    exe.max_memory = 64 * 1024 * 1024;
    exe.shared_memory = true;
    exe.stack_size = 8 * 1024 * 1024;

    b.getInstallStep().dependOn(&b.addInstallFileWithDir(b.path("../pacc/pacc-js.js"), .prefix, "pacc-js.js").step);
    const static_files: []const []const u8 = &.{
        "index.html",
        "index.js",
        "audio.js",
        "wasi.js",
    };
    for (static_files) |static_file| {
        b.getInstallStep().dependOn(&b.addInstallFileWithDir(b.path(static_file), .prefix, static_file).step);
    }
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(exe.getEmittedBin(), .prefix, "main.wasm").step);
}
