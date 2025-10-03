const HEADER = `
#version 100
#define VERT_IN attribute
#define VERT_OUT varying
#define FRAG_IN varying
#define TEXTURE2D texture2D
#define FRAGCOLOR_DECL
#define FRAGCOLOR gl_FragColor
`;

const VERTEX_SHADER = HEADER + `
VERT_IN vec4 coord;
VERT_OUT vec2 texcoord;
void main(void) {
  gl_Position = vec4(coord.xy, 0.0, 1.0);
  texcoord = coord.zw;
}
`;

const FRAGMENT_SHADER_COPY = HEADER + `
uniform sampler2D palette;
uniform sampler2D tex;
FRAG_IN mediump vec2 texcoord;
FRAGCOLOR_DECL
void main(void) {
  lowp float index = TEXTURE2D(tex, texcoord).x;
  lowp float color = (index * 255.0 + 0.5) / 256.0;
  FRAGCOLOR = TEXTURE2D(palette, vec2(color, 0.0));
}
`;

const FRAGMENT_SHADER_COLOR = HEADER + `
uniform sampler2D palette;
uniform sampler2D tex;
FRAG_IN mediump vec2 texcoord;
uniform lowp float color;
FRAGCOLOR_DECL
void main(void) {
  lowp float index = TEXTURE2D(tex, texcoord).x;
  if (index > (0.5/255.0)) {
    index = color;
  } else {
    index = 0.5 / 256.0;
  }
  FRAGCOLOR = TEXTURE2D(palette, vec2(index, 0.0));
}
`;

const FRAGMENT_SHADER_COLOR_TRANS = HEADER + `
uniform sampler2D palette;
uniform sampler2D tex;
FRAG_IN mediump vec2 texcoord;
uniform lowp float color;
FRAGCOLOR_DECL
void main(void) {
  lowp float index = TEXTURE2D(tex, texcoord).x;
  if (index < (0.5/255.0)) {
    discard;
  }
  FRAGCOLOR = TEXTURE2D(palette, vec2(color, 0.0));
}
`;

const VAI_COORD = 0;

/**
 * @param {WebAssembly.Memory} memory
 * @param {WebGLRenderingContext} gl 
 */
export function imports(memory, gl) {
  let progs;
  let uniColor;
  let uniColorTrans;
  const bufs = [];
  const texs = [];
  let color = 0;

  return {
    init() {
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

      const texPal = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, texPal);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

      progs = [
        compileAndLink(gl, VERTEX_SHADER, FRAGMENT_SHADER_COPY),
        compileAndLink(gl, VERTEX_SHADER, FRAGMENT_SHADER_COLOR),
        compileAndLink(gl, VERTEX_SHADER, FRAGMENT_SHADER_COLOR_TRANS),
      ];
      uniColor = gl.getUniformLocation(progs[1], "color");
      uniColorTrans = gl.getUniformLocation(progs[2], "color");
      
      gl.activeTexture(gl.TEXTURE1);
    },

    genBuf() {
      let pb = bufs.findIndex((buf) => !buf);
      if (pb === -1) {
        pb = bufs.length;
        bufs.push(null);
      }
      bufs[pb] = gl.createBuffer();
      return pb;
    },

    bufDelete(pb) {
      bufs[pb] = null;
    },

    bufUpdate(pb, bufPtr, len, mode) {
      gl.bindBuffer(gl.ARRAY_BUFFER, bufs[pb]);
      const data = new Float32Array(len);
      const memView = new DataView(memory.buffer);
      for (let i = 0; i < len; i++) {
        data[i] = memView.getFloat32(bufPtr + 4 * i, true);
      }
      gl.bufferData(gl.ARRAY_BUFFER, data, mode === 0 ? gl.STATIC_DRAW : gl.STREAM_DRAW);
    },

    palette(rgbPtr, colors) {
      const pal = new Uint8Array(256 * 3);
      pal.set(new Uint8Array(memory.buffer, rgbPtr, colors * 3));

      gl.activeTexture(gl.TEXTURE0);
      gl.texImage2D(
        gl.TEXTURE_2D,
        0, gl.RGB,
        256, 1,
        0, gl.RGB,
        gl.UNSIGNED_BYTE, pal,
      );
      gl.activeTexture(gl.TEXTURE1);

      gl.clearColor(pal[0] / 255, pal[1] / 255, pal[2] / 255, 1);
    },

    color(pal) {
      color = pal;
    },

    clear() {
      gl.clear(gl.COLOR_BUFFER_BIT);
    },

    draw(pt, pb, n, mode) {
      gl.useProgram(progs[mode]);
      switch (mode) {
      case 1:
        gl.uniform1f(uniColor, color / 255);
        break;
      case 2:
        gl.uniform1f(uniColorTrans, color / 255);
        break;
      }
      gl.bindTexture(gl.TEXTURE_2D, texs[pt]);
      gl.enableVertexAttribArray(VAI_COORD);
      gl.bindBuffer(gl.ARRAY_BUFFER, bufs[pb]);
      gl.vertexAttribPointer(VAI_COORD, 4, gl.FLOAT, false, 0, 0);
      gl.drawArrays(gl.TRIANGLES, 0, n);
    },

    genTex(w, h) {
      let pt = texs.findIndex((tex) => !tex);
      if (pt === -1) {
        pt = texs.length;
        texs.push(null);
      }
      texs[pt] = gl.createTexture();

      gl.bindTexture(gl.TEXTURE_2D, texs[pt]);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      if ((w & (w - 1)) || (h & (h - 1))) {
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      }

      return pt;
    },

    texDelete(pt) {
      texs[pt] = null;
    },

    texUpdate(pt, bufPtr, w, h) {
      gl.bindTexture(gl.TEXTURE_2D, texs[pt]);
      const buf = new Uint8Array(memory.buffer, bufPtr, w * h);
      gl.texImage2D(
        gl.TEXTURE_2D,
        0, gl.LUMINANCE,
        w, h, 0,
        gl.LUMINANCE, gl.UNSIGNED_BYTE,
        buf,
      );
    },
  };
}

/**
 * @param {WebGLRenderingContext} gl 
 * @param {string} ss
 * @param {number} type
 */
function compileShader(gl, ss, type) {
  const s = gl.createShader(type);
  gl.shaderSource(s, ss);
  gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
    throw new Error(`shader error: ${gl.getShaderInfoLog(s)}`);
  }
  return s;
}

/**
 * @param {WebGLRenderingContext} gl 
 * @param {string} vss
 * @param {string} fss
 */
function compileAndLink(gl, vss, fss) {
  const p = gl.createProgram();
  const fs = compileShader(gl, vss, gl.VERTEX_SHADER);
  const vs = compileShader(gl, fss, gl.FRAGMENT_SHADER);
  gl.attachShader(p, vs);
  gl.attachShader(p, fs);
  gl.bindAttribLocation(p, VAI_COORD, "coord");
  gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
    throw new Error(`program link error: ${gl.getProgramInfoLog(p)}`);
  }
  gl.useProgram(p);
  gl.uniform1i(gl.getUniformLocation(p, "tex"), 1);
  return p;
}
