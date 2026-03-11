struct VertexOutput {
  @builtin(position) pos: vec4f,
}

@vertex
fn main(@builtin(vertex_index) idx: u32) -> VertexOutput {
  var out: VertexOutput;
  let x = f32((idx << 1u) & 2u);
  let y = f32(idx & 2u);
  out.pos = vec4f(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
  return out;
}
