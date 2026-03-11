#include "gpu_context.h"
#include <GLFW/glfw3.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu/WebGPU.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#if defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace somarender {

namespace {

void onDeviceError(WGPUErrorType type, char const* message, void* userdata) {
  (void)userdata;
  fprintf(stderr, "[WebGPU] %s\n", message);
}

WGPUSurface createSurface(WGPUInstance instance, GLFWwindow* window) {
#if defined(_WIN32)
  WGPUSurfaceDescriptorFromWindowsHWND desc = {};
  desc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
  desc.hwnd = glfwGetWin32Window(window);
  desc.hinstance = GetModuleHandle(nullptr);

  WGPUSurfaceDescriptor surfDesc = {};
  surfDesc.nextInChain = &desc.chain;
  return wgpuInstanceCreateSurface(instance, &surfDesc);
#elif defined(__linux__)
  WGPUSurfaceDescriptorFromXlibWindow desc = {};
  desc.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
  desc.display = glfwGetX11Display();
  desc.window = glfwGetX11Window(window);

  WGPUSurfaceDescriptor surfDesc = {};
  surfDesc.nextInChain = &desc.chain;
  return wgpuInstanceCreateSurface(instance, &surfDesc);
#else
  (void)instance;
  (void)window;
  return nullptr;
#endif
}

}  // namespace

GPUContext::~GPUContext() {
  if (currentView_) {
    wgpuTextureViewRelease(currentView_);
    currentView_ = nullptr;
  }
  if (swapChain_) {
    wgpuSwapChainRelease(swapChain_);
    swapChain_ = nullptr;
  }
  if (surface_) {
    wgpuSurfaceRelease(surface_);
    surface_ = nullptr;
  }
  if (queue_) {
    wgpuQueueRelease(queue_);
    queue_ = nullptr;
  }
  if (device_) {
    wgpuDeviceRelease(device_);
    device_ = nullptr;
  }
  if (adapter_) {
    wgpuAdapterRelease(adapter_);
    adapter_ = nullptr;
  }
  if (instance_) {
    wgpuInstanceRelease(instance_);
    instance_ = nullptr;
  }
}

bool GPUContext::init(GLFWwindow* window) {
  window_ = window;
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  width_ = static_cast<uint32_t>(w > 0 ? w : 800);
  height_ = static_cast<uint32_t>(h > 0 ? h : 600);

  dawn::native::Instance dawnInstance;
  instance_ = dawnInstance.Get();  // some Dawn builds: GetWGPUInstance()
  if (!instance_) {
    return false;
  }
  wgpuInstanceReference(instance_);

  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
  adapterOpts.backendType = WGPUBackendType_Undefined;

  struct AdapterUserdata {
    WGPUAdapter adapter = nullptr;
    bool done = false;
  } adapterUserdata;

  wgpuInstanceRequestAdapter(instance_, &adapterOpts,
    [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
      auto* u = static_cast<AdapterUserdata*>(userdata);
      if (status == WGPURequestAdapterStatus_Success && adapter) {
        u->adapter = adapter;
      }
      if (message) {
        fprintf(stderr, "[Adapter] %s\n", message);
      }
      u->done = true;
    },
    &adapterUserdata);

  while (!adapterUserdata.done) {
    dawnInstance.ProcessEvents();
  }
  if (!adapterUserdata.adapter) {
    return false;
  }
  adapter_ = adapterUserdata.adapter;

  WGPUDeviceDescriptor deviceDesc = {};

  struct DeviceUserdata {
    WGPUDevice device = nullptr;
    bool done = false;
  } deviceUserdata;

  wgpuAdapterRequestDevice(adapter_, &deviceDesc,
    [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
      auto* u = static_cast<DeviceUserdata*>(userdata);
      if (status == WGPURequestDeviceStatus_Success && device) {
        u->device = device;
      }
      if (message) {
        fprintf(stderr, "[Device] %s\n", message);
      }
      u->done = true;
    },
    &deviceUserdata);

  while (!deviceUserdata.done) {
    dawnInstance.ProcessEvents();
  }
  if (!deviceUserdata.device) {
    return false;
  }
  device_ = deviceUserdata.device;

  wgpuDeviceSetUncapturedErrorCallback(device_, onDeviceError, nullptr);
  queue_ = wgpuDeviceGetQueue(device_);
  wgpuQueueReference(queue_);

  surface_ = createSurface(instance_, window_);
  if (!surface_) {
    return false;
  }

  WGPUSwapChainDescriptor swapDesc = {};
  swapDesc.usage = WGPUTextureUsage_RenderAttachment;
  swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
  swapDesc.width = width_;
  swapDesc.height = height_;
  swapDesc.presentMode = WGPUPresentMode_Fifo;
  swapChain_ = wgpuDeviceCreateSwapChain(device_, surface_, &swapDesc);
  return swapChain_ != nullptr;
}

void GPUContext::resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  width_ = width;
  height_ = height;
  if (currentView_) {
    wgpuTextureViewRelease(currentView_);
    currentView_ = nullptr;
  }
  if (swapChain_) {
    wgpuSwapChainRelease(swapChain_);
  }
  WGPUSwapChainDescriptor swapDesc = {};
  swapDesc.usage = WGPUTextureUsage_RenderAttachment;
  swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
  swapDesc.width = width_;
  swapDesc.height = height_;
  swapDesc.presentMode = WGPUPresentMode_Fifo;
  swapChain_ = wgpuDeviceCreateSwapChain(device_, surface_, &swapDesc);
}

bool GPUContext::beginFrame() {
  if (currentView_) {
    wgpuTextureViewRelease(currentView_);
    currentView_ = nullptr;
  }
  currentView_ = wgpuSwapChainGetCurrentTextureView(swapChain_);
  return currentView_ != nullptr;
}

void GPUContext::present() {
  wgpuSwapChainPresent(swapChain_);
}

WGPUShaderModule GPUContext::createShaderModule(const std::string& wgslSource) {
  WGPUShaderModuleWGSLDescriptor wgslDesc = {};
  wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDesc.code = wgslSource.c_str();

  WGPUShaderModuleDescriptor desc = {};
  desc.nextInChain = &wgslDesc.chain;
  return wgpuDeviceCreateShaderModule(device_, &desc);
}

WGPUShaderModule GPUContext::createShaderModuleFromFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) {
    return nullptr;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return createShaderModule(ss.str());
}

}  // namespace somarender
