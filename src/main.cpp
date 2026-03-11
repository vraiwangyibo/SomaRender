#include "gpu_context.h"
#include "volume_loader.h"
#include "camera.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

const char* kVertPath = "shaders/volume_raster.vert.wgsl";
const char* kFragPath = "shaders/volume_raycast.frag.wgsl";

struct UniformParams {
  float invViewProj[16];
  float cameraPos[3];
  float _pad0;
  float resolution[2];
  float _pad1[2];
  float volumeSize[3];
  float stepSize;
  float _pad2[2];
};
static_assert(sizeof(UniformParams) == 128, "Uniform layout for WGSL");

std::string readFile(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return "";
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::string out(static_cast<size_t>(size), '\0');
  size_t n = fread(&out[0], 1, static_cast<size_t>(size), f);
  fclose(f);
  out.resize(n);
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const char* volumePath = (argc > 1) ? argv[1] : "assets/volume.raw";
  uint32_t volW = 256, volH = 256, volD = 256;
  if (argc >= 5) {
    volW = static_cast<uint32_t>(atoi(argv[2]));
    volH = static_cast<uint32_t>(atoi(argv[3]));
    volD = static_cast<uint32_t>(atoi(argv[4]));
  }

  if (!glfwInit()) {
    fprintf(stderr, "glfwInit failed\n");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1024, 768, "SomaRender", nullptr, nullptr);
  if (!window) {
    fprintf(stderr, "glfwCreateWindow failed\n");
    glfwTerminate();
    return 1;
  }

  somarender::GPUContext gpu;
  if (!gpu.init(window)) {
    fprintf(stderr, "GPUContext init failed\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  somarender::VolumeLoader loader;
  if (!loader.loadRaw(volumePath, volW, volH, volD, false)) {
    fprintf(stderr, "Failed to load volume: %s (try: %u %u %u)\n", volumePath, volW, volH, volD);
  }
  WGPUTexture volumeTexture = loader.uploadToTexture(gpu.device(), gpu.queue());
  if (!volumeTexture) {
    fprintf(stderr, "Failed to upload volume texture\n");
    wgpuDeviceRelease(gpu.device());
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  std::string vertSrc = readFile(kVertPath);
  std::string fragSrc = readFile(kFragPath);
  if (vertSrc.empty() || fragSrc.empty()) {
    fprintf(stderr, "Failed to load shaders (look for %s, %s)\n", kVertPath, kFragPath);
    wgpuTextureDestroy(volumeTexture);
    wgpuTextureRelease(volumeTexture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  WGPUShaderModule vertMod = gpu.createShaderModule(vertSrc);
  WGPUShaderModule fragMod = gpu.createShaderModule(fragSrc);
  if (!vertMod || !fragMod) {
    fprintf(stderr, "Failed to create shader modules\n");
    if (vertMod) wgpuShaderModuleRelease(vertMod);
    if (fragMod) wgpuShaderModuleRelease(fragMod);
    wgpuTextureDestroy(volumeTexture);
    wgpuTextureRelease(volumeTexture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  WGPUSamplerDescriptor sampDesc = {};
  sampDesc.minFilter = WGPUFilterMode_Linear;
  sampDesc.magFilter = WGPUFilterMode_Linear;
  WGPUSampler sampler = wgpuDeviceCreateSampler(gpu.device(), &sampDesc);

  WGPUBufferDescriptor ubufDesc = {};
  ubufDesc.size = sizeof(UniformParams);
  ubufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  WGPUBuffer uniformBuffer = wgpuDeviceCreateBuffer(gpu.device(), &ubufDesc);

  WGPUBindGroupLayoutEntry bglEntries[3] = {};
  bglEntries[0].binding = 0;
  bglEntries[0].visibility = WGPUShaderStage_Fragment;
  bglEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
  bglEntries[0].texture.viewDimension = WGPUTextureViewDimension_3D;
  bglEntries[1].binding = 1;
  bglEntries[1].visibility = WGPUShaderStage_Fragment;
  bglEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;
  bglEntries[2].binding = 2;
  bglEntries[2].visibility = WGPUShaderStage_Fragment;
  bglEntries[2].buffer.type = WGPUBufferBindingType_Uniform;
  bglEntries[2].buffer.minBindingSize = sizeof(UniformParams);

  WGPUBindGroupLayoutDescriptor bglDesc = {};
  bglDesc.entryCount = 3;
  bglDesc.entries = bglEntries;
  WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(gpu.device(), &bglDesc);

  WGPUPipelineLayoutDescriptor plDesc = {};
  plDesc.bindGroupLayoutCount = 1;
  plDesc.bindGroupLayouts = &bgl;
  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(gpu.device(), &plDesc);

  WGPUVertexState vertState = {};
  vertState.module = vertMod;
  vertState.entryPoint = "main";

  WGPUFragmentState fragState = {};
  fragState.module = fragMod;
  fragState.entryPoint = "main";
  WGPUColorTargetState colorTarget = {};
  colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
  fragState.targetCount = 1;
  fragState.targets = &colorTarget;

  WGPURenderPipelineDescriptor rpDesc = {};
  rpDesc.vertex = vertState;
  rpDesc.fragment = &fragState;
  rpDesc.layout = pipelineLayout;
  rpDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(gpu.device(), &rpDesc);

  WGPUTextureView volumeView = wgpuTextureCreateView(volumeTexture, nullptr);

  WGPUBindGroupEntry bgEntries[3] = {};
  bgEntries[0].binding = 0;
  bgEntries[0].textureView = volumeView;
  bgEntries[1].binding = 1;
  bgEntries[1].sampler = sampler;
  bgEntries[2].binding = 2;
  bgEntries[2].buffer = uniformBuffer;
  bgEntries[2].offset = 0;
  bgEntries[2].size = sizeof(UniformParams);

  WGPUBindGroupDescriptor bgDesc = {};
  bgDesc.layout = bgl;
  bgDesc.entryCount = 3;
  bgDesc.entries = bgEntries;
  WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(gpu.device(), &bgDesc);

  somarender::Camera camera;
  bool mouseDown = false;
  double lastX = 0, lastY = 0;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW > 0 && fbH > 0 && (static_cast<uint32_t>(fbW) != gpu.width() || static_cast<uint32_t>(fbH) != gpu.height())) {
      gpu.resize(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      if (mouseDown) {
        camera.orbit(static_cast<float>(x - lastX) * 0.01f, static_cast<float>(lastY - y) * 0.01f);
      }
      mouseDown = true;
      lastX = x;
      lastY = y;
    } else {
      mouseDown = false;
    }
    double scroll = 0;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) scroll += 0.1f;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) scroll -= 0.1f;
    camera.zoom(static_cast<float>(scroll));

    UniformParams params = {};
    camera.getInvViewProjAndCameraPos(params.invViewProj, params.cameraPos, gpu.width(), gpu.height());
    params.resolution[0] = static_cast<float>(gpu.width());
    params.resolution[1] = static_cast<float>(gpu.height());
    const auto& meta = loader.metadata();
    params.volumeSize[0] = static_cast<float>(meta.width);
    params.volumeSize[1] = static_cast<float>(meta.height);
    params.volumeSize[2] = static_cast<float>(meta.depth);
    params.stepSize = 0.002f;

    wgpuQueueWriteBuffer(gpu.queue(), uniformBuffer, 0, &params, sizeof(params));

    if (!gpu.beginFrame()) continue;
    WGPUTextureView backbuffer = gpu.currentView();
    if (!backbuffer) continue;

    WGPURenderPassColorAttachment colorAtt = {};
    colorAtt.view = backbuffer;
    colorAtt.loadOp = WGPULoadOp_Clear;
    colorAtt.storeOp = WGPUStoreOp_Store;
    colorAtt.clearValue = { 0.05f, 0.05f, 0.08f, 1.0f };

    WGPURenderPassDescriptor rpPass = {};
    rpPass.colorAttachmentCount = 1;
    rpPass.colorAttachments = &colorAtt;

    WGPUCommandEncoderDescriptor encDesc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(gpu.device(), &encDesc);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpPass);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmdBufDesc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(gpu.queue(), 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    gpu.present();
  }

  wgpuBindGroupRelease(bindGroup);
  wgpuTextureViewRelease(volumeView);
  wgpuTextureDestroy(volumeTexture);
  wgpuTextureRelease(volumeTexture);
  wgpuRenderPipelineRelease(pipeline);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuBindGroupLayoutRelease(bgl);
  wgpuBufferDestroy(uniformBuffer);
  wgpuBufferRelease(uniformBuffer);
  wgpuSamplerRelease(sampler);
  wgpuShaderModuleRelease(vertMod);
  wgpuShaderModuleRelease(fragMod);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
