#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>
#include <string>
#include <vector>

namespace somarender {

struct VolumeMetadata {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  bool is16Bit = false;
};

class VolumeLoader {
public:
  bool loadRaw(const std::string& path, uint32_t width, uint32_t height, uint32_t depth, bool bits16 = false);
  bool loadRawWithHeader(const std::string& path);

  const std::vector<uint8_t>& data() const { return data_; }
  const VolumeMetadata& metadata() const { return meta_; }

  WGPUTexture uploadToTexture(WGPUDevice device, WGPUQueue queue) const;

private:
  std::vector<uint8_t> data_;
  VolumeMetadata meta_;
};

}  // namespace somarender
