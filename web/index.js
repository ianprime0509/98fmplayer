import { imports as paccImports } from "./pacc-webgl.js";
import { imports as wasiImports } from "./wasi.js";

const canvas = document.getElementById("fmdsp");
const gl = canvas.getContext("webgl");

const pacc = paccImports(gl);
const wasi = wasiImports();

const wasm = await WebAssembly.instantiateStreaming(fetch("main.wasm"), {
  pacc,
  wasi_snapshot_preview1: wasi,
}).then((w) => w.instance);

pacc.initWasm(wasm.exports.memory);
wasi.initWasm(wasm.exports.memory);

if (wasm.exports.init() !== 1) throw new Error("init failed");

function render() {
  wasm.exports.render();
  requestAnimationFrame(render);
}

requestAnimationFrame(render);

const audioCtx = new AudioContext({
  sampleRate: 55467,
});
const scriptNode = audioCtx.createScriptProcessor(1024, 0, 2);
scriptNode.addEventListener("audioprocess", (ev) => {
  const samples = ev.outputBuffer.getChannelData(0).length;
  wasm.exports.mix(samples);
  const audio = new DataView(wasm.exports.memory.buffer, wasm.exports.getAudioBuf(), samples * 4);
  for (let i = 0; i < samples; i++) {
    ev.outputBuffer.getChannelData(0)[i] = audio.getInt16(4 * i, true) / 32767;
    ev.outputBuffer.getChannelData(1)[i] = audio.getInt16(4 * i + 2, true) / 32767;
  }
});
scriptNode.connect(audioCtx.destination);

const fileInput = document.getElementById("file");
fileInput.addEventListener("change", (ev) => {
  const reader = new FileReader();
  reader.onload = () => {
    console.log(wasm.exports.getFileBuf());
    const fileBuf = new Uint8Array(wasm.exports.memory.buffer, wasm.exports.getFileBuf(), 0xffff);
    fileBuf.set(new Uint8Array(reader.result));
    wasm.exports.loadFile(reader.result.byteLength);
    audioCtx.resume();
  };
  reader.readAsArrayBuffer(fileInput.files[0]);
});
