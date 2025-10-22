import { imports as fileImports } from "./fmplayer_file_js.js";
import { imports as platformImports } from "./fmdsp_platform_js.js";
import { imports as paccImports } from "./pacc-js.js";
import { imports as wasiImports } from "./wasi.js";

const files = {};
const fmdspEvents = new EventTarget();

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
  fmplayer_file: fileImports(memory, files),
  fmdsp_platform: platformImports(),
  pacc: paccImports(memory, gl),
  wasi_snapshot_preview1: wasiImports(memory),
}).then((r) => r.instance);
wasm.exports._initialize();

if (wasm.exports.init() !== 1) throw new Error("init failed");

let wasPlaying = false;
function render() {
  wasm.exports.render();
  const playing = wasm.exports.playing();
  if (wasPlaying && !playing) fmdspEvents.dispatchEvent(new Event("stopped"));
  wasPlaying = playing;
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
const songSelect = document.getElementById("song-select");
songSelect.addEventListener("change", () => {
  const reader = new FileReader();
  reader.addEventListener("load", () => {
    const filename = songSelect.files[0].name;
    for (const file in files) delete files[file];
    files[filename] = new Uint8Array(reader.result);
    const filenameBuf = new Uint8Array(memory.buffer, wasm.exports.getFilenameBuf(), 128);
    filenameBuf.set(utf8Encoder.encode(filename + "\0"));
    wasm.exports.loadFile();
    audioCtx.resume();
  });
  reader.readAsArrayBuffer(songSelect.files[0]);
});

canvas.addEventListener("click", () => wasm.exports.togglePaused());
canvas.addEventListener("keydown", (ev) => {
  switch (ev.key) {
  case " ":
    wasm.exports.togglePaused();
    break;
  }
});
