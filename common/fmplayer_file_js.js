/**
 * @param {WebAssembly.Memory} memory
 * @param {Record<string, Uint8Array>} files
 */
export function imports(memory, files) {
  const readPath = (ptr) => {
    let path = new Uint8Array(memory.buffer, ptr);
    path = path.slice(0, path.findIndex((n) => n === 0));
    return new TextDecoder().decode(path);
  };
  const getFile = (path) => Object.entries(files)
    .find(([filename]) => path.toUpperCase() === filename.toUpperCase())
    ?.[1];

  return {
    size(pathPtr) {
      const file = getFile(readPath(pathPtr));
      return file ? file.length : -1;
    },

    read(pathPtr, bufPtr) {
      const file = getFile(readPath(pathPtr));
      const buf = new Uint8Array(memory.buffer, bufPtr);
      buf.set(file);
    },
  };
}
