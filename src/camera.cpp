#include "camera.h"
#include <cstring>

namespace somarender {

namespace {

void setIdentity(float* m) {
  for (int i = 0; i < 16; ++i) m[i] = 0;
  m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void multiply4x4(const float* a, const float* b, float* out) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      out[row * 4 + col] = a[row * 4 + 0] * b[0 * 4 + col] +
                           a[row * 4 + 1] * b[1 * 4 + col] +
                           a[row * 4 + 2] * b[2 * 4 + col] +
                           a[row * 4 + 3] * b[3 * 4 + col];
    }
  }
}

void invert4x4(const float* m, float* out) {
  float inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (std::abs(det) < 1e-8f) {
    setIdentity(out);
    return;
  }
  det = 1.0f / det;
  for (int i = 0; i < 16; ++i) out[i] = inv[i] * det;
}

}  // namespace

void Camera::orbit(float dAzimuth, float dElevation) {
  azimuth += dAzimuth;
  elevation += dElevation;
  const float maxElev = 3.14159f * 0.5f - 0.01f;
  if (elevation > maxElev) elevation = maxElev;
  if (elevation < -maxElev) elevation = -maxElev;
}

void Camera::zoom(float delta) {
  distance -= delta;
  if (distance < 0.5f) distance = 0.5f;
  if (distance > 20.0f) distance = 20.0f;
}

void Camera::getViewProjection(float* view4x4, float* proj4x4, uint32_t viewportWidth, uint32_t viewportHeight) const {
  float cx = std::cos(azimuth);
  float sx = std::sin(azimuth);
  float cy = std::cos(elevation);
  float sy = std::sin(elevation);
  float eyeX = distance * cx * cy;
  float eyeY = distance * sy;
  float eyeZ = distance * sx * cy;
  float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
  float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

  float f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
  float len = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  if (len > 1e-6f) {
    f[0] /= len; f[1] /= len; f[2] /= len;
  }
  float s[3] = {
    f[1] * upZ - f[2] * upY,
    f[2] * upX - f[0] * upZ,
    f[0] * upY - f[1] * upX
  };
  len = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
  if (len > 1e-6f) {
    s[0] /= len; s[1] /= len; s[2] /= len;
  }
  float u[3] = {
    s[1] * f[2] - s[2] * f[1],
    s[2] * f[0] - s[0] * f[2],
    s[0] * f[1] - s[1] * f[0]
  };

  view4x4[0] = s[0];  view4x4[4] = s[1];  view4x4[8]  = s[2];  view4x4[12] = -(s[0] * eyeX + s[1] * eyeY + s[2] * eyeZ);
  view4x4[1] = u[0];  view4x4[5] = u[1];  view4x4[9]  = u[2];  view4x4[13] = -(u[0] * eyeX + u[1] * eyeY + u[2] * eyeZ);
  view4x4[2] = -f[0]; view4x4[6] = -f[1]; view4x4[10] = -f[2]; view4x4[14] = (f[0] * eyeX + f[1] * eyeY + f[2] * eyeZ);
  view4x4[3] = 0;     view4x4[7] = 0;     view4x4[11] = 0;     view4x4[15] = 1;

  float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight > 0u ? viewportHeight : 1u);
  float tanHalfFov = std::tan(fovYRad * 0.5f);
  float n = near, f = far;
  proj4x4[0] = 1.0f / (aspect * tanHalfFov); proj4x4[4] = 0;                        proj4x4[8]  = 0;                          proj4x4[12] = 0;
  proj4x4[1] = 0;                             proj4x4[5] = 1.0f / tanHalfFov;       proj4x4[9]  = 0;                          proj4x4[13] = 0;
  proj4x4[2] = 0;                             proj4x4[6] = 0;                        proj4x4[10] = -(f + n) / (f - n);          proj4x4[14] = -(2 * f * n) / (f - n);
  proj4x4[3] = 0;                             proj4x4[7] = 0;                        proj4x4[11] = -1;                         proj4x4[15] = 0;
}

void Camera::getInvViewProjAndCameraPos(float* invViewProj4x4, float* cameraPos3, uint32_t viewportWidth, uint32_t viewportHeight) const {
  float view[16], proj[16];
  getViewProjection(view, proj, viewportWidth, viewportHeight);
  float viewProj[16];
  multiply4x4(proj, view, viewProj);
  invert4x4(viewProj, invViewProj4x4);
  float invView[16];
  invert4x4(view, invView);
  cameraPos3[0] = invView[12];
  cameraPos3[1] = invView[13];
  cameraPos3[2] = invView[14];
}

}  // namespace somarender
