const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    if (b.systemIntegrationOption("portaudio", .{})) {
        mod.linkSystemLibrary("portaudio", .{});
    } else portaudio: {
        const portaudio_dep = b.lazyDependency("portaudio", .{
            .target = target,
            .optimize = optimize,
        }) orelse break :portaudio;
        mod.linkLibrary(portaudio_dep.artifact("portaudio"));
    }

    mod.linkLibrary(libpmdmc(b, target, optimize));

    mod.addCMacro("_POSIX_C_SOURCE", "200809L");
    mod.addIncludePath(b.path(".."));
    mod.addCSourceFiles(.{
        .root = b.path(".."),
        .files = &.{
            "cli/main.c",
            "common/fmplayer_file.c",
            "common/fmplayer_file_unix.c",
            "common/fmplayer_work_opna.c",
            "common/fmplayer_drumrom_unix.c",
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
        },
        .flags = &.{
            "-Wall",
            "-Wextra",
            "-pedantic",
            "-std=c99",
            "-fno-sanitize=shift",
        },
    });

    const exe = b.addExecutable(.{
        .name = "98fmplayer",
        .root_module = mod,
    });
    b.installArtifact(exe);
}

fn libpmdmc(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("pmdc", .{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addCMacro("lang", b.fmt("{}", .{256 * 'e' + 'n'}));
    mod.addCSourceFiles(.{
        .root = dep.path("src/mc"),
        .files = &.{
            "mc.c",
        },
        .flags = &.{
            "-Wall",
            "-Wextra",
            "-pedantic",
            "-std=c11",
        },
    });

    const lib = b.addLibrary(.{
        .name = "pmdmc",
        .root_module = mod,
    });
    lib.installHeader(dep.path("src/mc/mc.h"), "mc.h");
    return lib;
}
