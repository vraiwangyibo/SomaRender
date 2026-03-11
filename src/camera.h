#pragma once

#include <cmath>
#include <cstdint>

namespace somarender {

struct Camera {
  float distance = 2.5f;
  float azimuth = 0.0f;
  float elevation = 0.3f;
  float fovYRad = 0.8f;
  float near = 0.01f;
  float far = 100.0f;

  void orbit(float dAzimuth, float dElevation);
  void zoom(float delta);
  void getViewProjection(float* view4x4, float* proj4x4, uint32_t viewportWidth, uint32_t viewportHeight) const;
  void getInvViewProjAndCameraPos(float* invViewProj4x4, float* cameraPos3, uint32_t viewportWidth, uint32_t viewportHeight) const;
};

}  // namespace somarender
