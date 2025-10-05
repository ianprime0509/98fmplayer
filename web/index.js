import { imports as paccImports } from "./pacc-js.js";
import { imports as wasiImports } from "./wasi.js";

const canvas = document.getElementById("fmdsp");
const gl = canvas.getContext("webgl");

const source = await fetch("main.wasm").then((r) => r.arrayBuffer());
const memory = new WebAssembly.Memory({
  initial: 1 * 1024,
  maximum: 1 * 1024,
  shared: true,
});
const wasm = await WebAssembly.instantiate(source, {
  env: { memory },
  pacc: paccImports(memory, gl),
  wasi_snapshot_preview1: wasiImports(memory),
}).then((r) => r.instance);
wasm.exports._initialize();

if (wasm.exports.init() !== 1) throw new Error("init failed");

function render() {
  wasm.exports.render();
  requestAnimationFrame(render);
}

requestAnimationFrame(render);

const audioCtx = new AudioContext({
  sampleRate: 55467,
});
await audioCtx.audioWorklet.addModule("audio.js");
const audioNode = new AudioWorkletNode(audioCtx, "audio", {
  numberOfInputs: 0,
  numberOfOutputs: 1,
  outputChannelCount: [2],
  processorOptions: { source, memory },
});
audioNode.connect(audioCtx.destination);

const utf8Encoder = new TextEncoder();
const fileInput = document.getElementById("file");
fileInput.addEventListener("change", () => {
  const reader = new FileReader();
  reader.onload = () => {
    const fileBuf = new Uint8Array(memory.buffer, wasm.exports.getFileBuf(), 0xffff);
    fileBuf.set(new Uint8Array(reader.result));
    // TODO: need to encode using Shift JIS
    const filenameBuf = new Uint8Array(memory.buffer, wasm.exports.getFilenameBuf(), 128);
    filenameBuf.set(utf8Encoder.encode(fileInput.files[0].name + "\0"));
    wasm.exports.loadFile(reader.result.byteLength);
    audioCtx.resume();
  };
  reader.readAsArrayBuffer(fileInput.files[0]);
});

const paletteInput = document.getElementById("palette");
paletteInput.addEventListener("change", () => {
  wasm.exports.setPalette(paletteInput.value - 1);
});

const body = document.getElementById("body");
body.addEventListener("keydown", (ev) => {
  switch (ev.key) {
  case " ":
    wasm.exports.togglePaused();
    break;
  case "ArrowDown":
    wasm.exports.commentScroll(true);
    break;
  case "ArrowUp":
    wasm.exports.commentScroll(false);
    break;
  }
});
