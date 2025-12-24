const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const cpu = target.result.cpu;
    const enable_neon = cpu.has(.arm, .neon) or cpu.has(.aarch64, .neon);
    const enable_sse = cpu.has(.x86, .sse2);
    const sdl_sys = b.systemIntegrationOption("sdl", .{});

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addCMacro("_POSIX_C_SOURCE", "200809L");
    mod.addCMacro("LIBOPNA_ENABLE_LEVELDATA", "");
    mod.addCMacro("PACC_GL_3", "");
    mod.linkSystemLibrary("gl", .{});
    if (sdl_sys) {
        mod.linkSystemLibrary("sdl3", .{});
    } else sdl: {
        const sdl_dep = b.lazyDependency("sdl", .{
            .target = target,
            .optimize = optimize,
        }) orelse break :sdl;
        mod.linkLibrary(sdl_dep.artifact("SDL3"));
    }
    var files: std.ArrayList([]const u8) = .empty;
    files.appendSlice(b.allocator, &.{
        "sdl/main.c",
        "common/fmplayer_file.c",
        "common/fmplayer_work_opna.c",
        "fft/fft.c",
        "fmdriver/fmdriver_common.c",
        "fmdriver/fmdriver_pmd.c",
        "fmdriver/fmdriver_fmp.c",
        "fmdriver/ppz8.c",
        "fmdsp/fmdsp-pacc.c",
        "fmdsp/font_fmdsp_small.c",
        "fmdsp/font_rom.c",
        "libopna/opna.c",
        "libopna/opnafm.c",
        "libopna/opnassg.c",
        "libopna/opnadrum.c",
        "libopna/opnaadpcm.c",
        "libopna/opnatimer.c",
        "libopna/opnassg-sinc-c.c",
        "pacc/pacc-gl.c",
    }) catch @panic("OOM");
    switch (target.result.os.tag) {
        .windows => {
            mod.addCMacro("FMPLAYER_FILE_WIN_UTF8", "");
            files.appendSlice(b.allocator, &.{
                "common/fmplayer_file_win.c",
                "common/fmplayer_drumrom_win.c",
                "common/fmplayer_fontrom_win.c",
                "fmdsp/fmdsp_platform_win.c",
            }) catch @panic("OOM");
        },
        .macos => {
            mod.addIncludePath(b.path("osx"));
            files.appendSlice(b.allocator, &.{
                "common/fmplayer_file_unix.c",
                "common/fmplayer_drumrom_unix.c",
                "common/fmplayer_fontrom_unix.c",
                "fmdsp/fmdsp_platform_mach.c",
            }) catch @panic("OOM");
        },
        else => {
            files.appendSlice(b.allocator, &.{
                "common/fmplayer_file_unix.c",
                "common/fmplayer_drumrom_unix.c",
                "common/fmplayer_fontrom_unix.c",
                "fmdsp/fmdsp_platform_unix.c",
            }) catch @panic("OOM");
        },
    }
    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path(".."));
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
            // We can't use -pedantic due to casts for OpenGL function pointers from void*
            "-Wno-unknown-attributes", // due to optimize attribute
            "-fno-sanitize=shift",
        },
    });

    const exe = b.addExecutable(.{
        .name = "98fmplayer",
        .root_module = mod,
    });
    b.installArtifact(exe);

    const bundle_step = b.step("bundle", "Create macOS application bundle");
    if (target.result.os.tag == .macos) {
        const bundle_dir: std.Build.InstallDir = .{ .custom = "98fmplayer.app" };
        const bundle_install_info = b.addInstallFileWithDir(b.path("osx/Info.plist"), bundle_dir, "Contents/Info.plist");
        bundle_step.dependOn(&bundle_install_info.step);
        const bundle_install_exe = b.addInstallFileWithDir(exe.getEmittedBin(), bundle_dir, "Contents/MacOS/98fmplayer");
        bundle_step.dependOn(&bundle_install_exe.step);
        if (sdl_sys) {
            const bundle_install_sdl_framework = b.addInstallDirectory(.{
                .source_dir = .{ .cwd_relative = "/Library/Frameworks/SDL3.framework" },
                .install_dir = bundle_dir,
                .install_subdir = "Contents/Frameworks/SDL3.framework",
            });
            bundle_step.dependOn(&bundle_install_sdl_framework.step);
        }
    } else {
        const bundle_unavailable = b.addFail("bundle is only available on macOS");
        bundle_step.dependOn(&bundle_unavailable.step);
    }
}
