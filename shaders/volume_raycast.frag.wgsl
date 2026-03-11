@group(0) @binding(0) var vol: texture_3d<f32>;
@group(0) @binding(1) var volSampler: sampler;
@group(0) @binding(2) var<uniform> params: Params;

struct Params {
  invViewProj: mat4x4f,
  cameraPos: vec3f,
  resolution: vec2f,
  volumeSize: vec3f,
  stepSize: f32,
}

fn rayBoxIntersect(rayOrigin: vec3f, rayDir: vec3f, boxMin: vec3f, boxMax: vec3f) -> vec2f {
  let invDir = 1.0 / rayDir;
  let t0 = (boxMin - rayOrigin) * invDir;
  let t1 = (boxMax - rayOrigin) * invDir;
  let tmin = min(t0, t1);
  let tmax = max(t0, t1);
  let tenter = max(max(tmin.x, tmin.y), tmin.z);
  let texit = min(min(tmax.x, tmax.y), tmax.z);
  if (tenter > texit || texit < 0.0) {
    return vec2f(-1.0, -1.0);
  }
  let tstart = max(0.0, tenter);
  let tend = texit;
  return vec2f(tstart, tend);
}

fn sampleVolume(uv: vec3f) -> f32 {
  return textureSample(vol, volSampler, uv).r;
}

fn applyTF(value: f32) -> vec4f {
  let opacity = value * 2.0;
  return vec4f(value, value, value * 1.2, opacity);
}

@fragment
fn main(@builtin(position) fragCoord: vec4f) -> @location(0) vec4f {
  let ndc = vec2f(fragCoord.xy) / params.resolution * 2.0 - 1.0;
  let clipNear = params.invViewProj * vec4f(ndc.x, ndc.y, 0.0, 1.0);
  let clipFar = params.invViewProj * vec4f(ndc.x, ndc.y, 1.0, 1.0);
  let worldNear = clipNear.xyz / clipNear.w;
  let worldFar = clipFar.xyz / clipFar.w;
  let rayOrigin = params.cameraPos;
  let rayDir = normalize(worldFar - worldNear);

  let boxMin = vec3f(0.0, 0.0, 0.0);
  let boxMax = vec3f(1.0, 1.0, 1.0);
  let hit = rayBoxIntersect(rayOrigin, rayDir, boxMin, boxMax);
  if (hit.x < 0.0) {
    return vec4f(0.05, 0.05, 0.08, 1.0);
  }

  var t = hit.x;
  let tend = hit.y;
  var color = vec4f(0.0, 0.0, 0.0, 0.0);
  let step = params.stepSize;

  while (t < tend && color.a < 0.95) {
    let worldPos = rayOrigin + t * rayDir;
    let uv = worldPos;
    let value = sampleVolume(uv);
    let sampleColor = applyTF(value);
    color.rgb += (1.0 - color.a) * sampleColor.a * sampleColor.rgb;
    color.a += (1.0 - color.a) * sampleColor.a;
    t += step;
  }

  return vec4f(color.rgb, 1.0);
}
