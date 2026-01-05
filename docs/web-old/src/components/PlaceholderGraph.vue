<template>
  <div class="w-screen h-screen">
    <canvas ref="canvas" class="w-full h-full"></canvas>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'

const canvas = ref<HTMLCanvasElement | null>(null)

onMounted(() => {
  const dpr = window.devicePixelRatio || 1
  const canvasEl = canvas.value

  if (!canvasEl) {
    console.error('Canvas element not found')
    return
  }

  canvasEl.width = canvasEl.clientWidth * dpr
  canvasEl.height = canvasEl.clientHeight * dpr

  const gl = canvasEl.getContext('webgl')
  if (!gl) {
    console.error('WebGL not supported')
    return
  }

  gl.viewport(0, 0, canvasEl.width, canvasEl.height)

  const vertexShaderSource = `
    attribute vec2 a_position;
    uniform float u_angle;
    uniform float u_aspect;
    void main() {
      float s = sin(u_angle);
      float c = cos(u_angle);
      vec2 rotated = vec2(
        a_position.x * c - a_position.y * s,
        a_position.x * s + a_position.y * c
      );
      vec2 scaled = vec2(rotated.x, rotated.y * u_aspect);
      gl_Position = vec4(scaled, 0, 1);
    }
`

  const fragmentShaderSource = `
    precision mediump float;
    void main() {
      gl_FragColor = vec4(0.2, 0.7, 1.0, 1.0);
    }
  `

  function createShader(gl: WebGLRenderingContext, type: number, src: string): WebGLShader | null {
    const shader = gl.createShader(type)
    if (!shader) return null
    gl.shaderSource(shader, src)
    gl.compileShader(shader)
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      console.error(gl.getShaderInfoLog(shader))
      gl.deleteShader(shader)
      return null
    }
    return shader
  }

  const vertexShader = createShader(gl, gl.VERTEX_SHADER, vertexShaderSource)
  const fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, fragmentShaderSource)
  if (!vertexShader || !fragmentShader) return

  function createProgram(gl: WebGLRenderingContext, vShader: WebGLShader, fShader: WebGLShader): WebGLProgram | null {
    const program = gl.createProgram()
    if (!program) return null
    gl.attachShader(program, vShader)
    gl.attachShader(program, fShader)
    gl.linkProgram(program)
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      console.error(gl.getProgramInfoLog(program))
      gl.deleteProgram(program)
      return null
    }
    return program
  }

  const program = createProgram(gl, vertexShader, fragmentShader)
  if (!program) return
  gl.useProgram(program)

  const points: number[] = [1, 4, 5, 1, 10, 0.1, 1.3, 5.1, 2, 20, 1, 2.3, -1, 1, 2, 3, 4, 5, 6, 7, 8, 1]
  const max = Math.max(...points)

  const vertexArray: number[] = []
  for (let i = 0; i < points.length - 1; i++) {
    const x1 = (i / (points.length - 1)) * 2 - 1
    const y1 = (points[i] / max) * 2 - 1
    const x2 = ((i + 1) / (points.length - 1)) * 2 - 1
    const y2 = (points[i + 1] / max) * 2 - 1
    vertexArray.push(x1, y1, x2, y2)
  }

  const vertices = new Float32Array(vertexArray)

  const buffer = gl.createBuffer()
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer)
  gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW)

  const posAttribLoc = gl.getAttribLocation(program, 'a_position')
  gl.enableVertexAttribArray(posAttribLoc)
  gl.vertexAttribPointer(posAttribLoc, 2, gl.FLOAT, false, 0, 0)

  const angleUniformLoc = gl.getUniformLocation(program, 'u_angle')
  const aspectUniformLoc = gl.getUniformLocation(program, 'u_aspect')

  if (!angleUniformLoc || !aspectUniformLoc) {
    console.error('Missing uniform location')
    return
  }

  function render(time: number) {
    const _gl = gl!;
    const aspect = _gl.canvas.height / _gl.canvas.width
    const angle = time * 0.001

    _gl.clearColor(0.05, 0.05, 0.05, 1.0)
    _gl.clear(_gl.COLOR_BUFFER_BIT)

    _gl.uniform1f(angleUniformLoc, angle)
    _gl.uniform1f(aspectUniformLoc, aspect)
    _gl.drawArrays(_gl.LINES, 0, vertices.length / 2)

    requestAnimationFrame(render)
  }

  requestAnimationFrame(render)
})
</script>

<style scoped>
  canvas {
    display: block;
    width: 100%;
    height: 100%;
  }
</style>

