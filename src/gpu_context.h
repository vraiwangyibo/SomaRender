#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

namespace somarender {

class GPUContext {
public:
  GPUContext() = default;
  ~GPUContext();

  bool init(GLFWwindow* window);
  void resize(uint32_t width, uint32_t height);

  WGPUDevice device() const { return device_; }
  WGPUQueue queue() const { return queue_; }
  bool beginFrame();
  WGPUTextureView currentView() const { return currentView_; }
  void present();

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

  WGPUShaderModule createShaderModule(const std::string& wgslSource);
  WGPUShaderModule createShaderModuleFromFile(const std::string& path);

private:
  WGPUInstance instance_ = nullptr;
  WGPUAdapter adapter_ = nullptr;
  WGPUDevice device_ = nullptr;
  WGPUQueue queue_ = nullptr;
  WGPUSurface surface_ = nullptr;
  WGPUSwapChain swapChain_ = nullptr;
  WGPUTextureView currentView_ = nullptr;

  uint32_t width_ = 0;
  uint32_t height_ = 0;
  GLFWwindow* window_ = nullptr;
};

}  // namespace somarender
