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

    if (b.systemIntegrationOption("sdl", .{})) {
        mod.linkSystemLibrary("sdl3", .{});
    } else sdl: {
        const sdl_dep = b.lazyDependency("sdl", .{
            .target = target,
            .optimize = optimize,
        }) orelse break :sdl;
        mod.linkLibrary(sdl_dep.artifact("SDL3"));
    }

    mod.linkLibrary(libpmdmc(b, target, optimize));

    mod.addCMacro("_POSIX_C_SOURCE", "200809L");
    mod.addIncludePath(b.path(".."));
    var files: std.ArrayList([]const u8) = .empty;
    files.appendSlice(b.allocator, &.{
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
