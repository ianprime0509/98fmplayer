const ENOTSUP = 58;

export function imports() {
  let mem;

  return {
    initWasm(wasmMemory) {
      mem = wasmMemory;
    },

    args_get() { return ENOTSUP; },
    args_sizes_get() { return ENOTSUP; },
    clock_res_get() { return ENOTSUP; },
    clock_time_get() { return ENOTSUP; },
    environ_get() { return ENOTSUP; },
    environ_sizes_get() { return ENOTSUP; },
    fd_advise() { return ENOTSUP; },
    fd_allocate() { return ENOTSUP; },
    fd_close() { return ENOTSUP; },
    fd_datasync() { return ENOTSUP; },
    fd_fdstat_get() { return ENOTSUP; },
    fd_fdstat_set_flags() { return ENOTSUP; },
    fd_fdstat_set_rights() { return ENOTSUP; },
    fd_filestat_get() { return ENOTSUP; },
    fd_filestat_set_size() { return ENOTSUP; },
    fd_filestat_set_times() { return ENOTSUP; },
    fd_pread() { return ENOTSUP; },
    fd_prestat_dir_name() { return ENOTSUP; },
    fd_prestat_get() { return ENOTSUP; },
    fd_pwrite() { return ENOTSUP; },
    fd_read() { return ENOTSUP; },
    fd_readdir() { return ENOTSUP; },
    fd_renumber() { return ENOTSUP; },
    fd_seek() { return ENOTSUP; },
    fd_sync() { return ENOTSUP; },
    fd_tell() { return ENOTSUP; },
    fd_write(fd, iovs, iovsLen, nWritten) {
      // Temporary stub implementation that writes nothing
      // TODO: use of Uint32Array won't work correctly on big endian
      const iovsData = new Uint32Array(wasm.exports.memory.buffer, iovs, iovsLen * 2);
      let n = 0;
      for (let i = 0; i < iovsLen; i++) {
        n += iovsData[2 * i + 1];
      }
      const retData = new Uint32Array(wasm.exports.memory.buffer, nWritten, 1);
      retData[0] = n;
      return 0;
    },
    path_create_directory() { return ENOTSUP; },
    path_filestat_get() { return ENOTSUP; },
    path_filestat_set_times() { return ENOTSUP; },
    path_link() { return ENOTSUP; },
    path_open() { return ENOTSUP; },
    path_readlink() { return ENOTSUP; },
    path_remove_directory() { return ENOTSUP; },
    path_rename() { return ENOTSUP; },
    path_symlink() { return ENOTSUP; },
    path_unlink_file() { return ENOTSUP; },
    poll_oneoff() { return ENOTSUP; },
    proc_exit() { return ENOTSUP; },
    random_get() { return ENOTSUP; },
    sched_yield() { return ENOTSUP; },
    sock_accept() { return ENOTSUP; },
    sock_recv() { return ENOTSUP; },
    sock_send() { return ENOTSUP; },
    sock_shutdown() { return ENOTSUP; },
  };
}
