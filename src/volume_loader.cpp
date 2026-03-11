#include "volume_loader.h"
#include <fstream>
#include <cstring>

namespace somarender {

bool VolumeLoader::loadRaw(const std::string& path, uint32_t width, uint32_t height, uint32_t depth, bool bits16) {
  meta_.width = width;
  meta_.height = height;
  meta_.depth = depth;
  meta_.is16Bit = bits16;

  size_t voxelBytes = bits16 ? 2u : 1u;
  size_t totalBytes = static_cast<size_t>(width) * height * depth * voxelBytes;

  std::ifstream f(path, std::ios::binary);
  if (!f || !f.seekg(0, std::ios::end)) {
    return false;
  }
  if (f.tellg() != static_cast<std::streampos>(totalBytes)) {
    return false;
  }
  f.seekg(0);
  data_.resize(totalBytes);
  if (!f.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(totalBytes))) {
    return false;
  }
  return true;
}

bool VolumeLoader::loadRawWithHeader(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f || !f.seekg(0, std::ios::end)) {
    return false;
  }
  size_t fileSize = static_cast<size_t>(f.tellg());
  f.seekg(0);
  const size_t headerSize = 16u;
  if (fileSize < headerSize) {
    return false;
  }
  uint32_t dims[3] = {0, 0, 0};
  uint32_t bits = 8;
  if (!f.read(reinterpret_cast<char*>(dims), 12) || !f.read(reinterpret_cast<char*>(&bits), 4)) {
    return false;
  }
  meta_.width = dims[0];
  meta_.height = dims[1];
  meta_.depth = dims[2];
  meta_.is16Bit = (bits == 16);
  size_t voxelBytes = meta_.is16Bit ? 2u : 1u;
  size_t totalBytes = static_cast<size_t>(meta_.width) * meta_.height * meta_.depth * voxelBytes;
  if (fileSize - headerSize != totalBytes) {
    return false;
  }
  data_.resize(totalBytes);
  return !!f.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(totalBytes));
}

WGPUTexture VolumeLoader::uploadToTexture(WGPUDevice device, WGPUQueue queue) const {
  if (data_.empty() || !device || !queue) {
    return nullptr;
  }

  const bool useR8 = true;
  std::vector<uint8_t> uploadData;
  const void* srcPtr = data_.data();
  size_t srcSize = data_.size();
  if (meta_.is16Bit && useR8) {
    uploadData.resize(static_cast<size_t>(meta_.width) * meta_.height * meta_.depth);
    const uint16_t* src = reinterpret_cast<const uint16_t*>(data_.data());
    for (size_t i = 0; i < uploadData.size(); ++i) {
      uploadData[i] = static_cast<uint8_t>(src[i] >> 8);
    }
    srcPtr = uploadData.data();
    srcSize = uploadData.size();
  }

  WGPUTextureDescriptor texDesc = {};
  texDesc.size = { meta_.width, meta_.height, meta_.depth };
  texDesc.dimension = WGPUTextureDimension_3D;
  texDesc.format = WGPUTextureFormat_R8Unorm;
  texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  texDesc.mipLevelCount = 1;
  texDesc.sampleCount = 1;
  WGPUTexture tex = wgpuDeviceCreateTexture(device, &texDesc);
  if (!tex) {
    return nullptr;
  }

  WGPUImageCopyTexture dst = {};
  dst.texture = tex;
  dst.origin = { 0, 0, 0 };
  dst.mipLevel = 0;

  uint32_t bytesPerPixel = 1u;
  WGPUTextureDataLayout layout = {};
  layout.bytesPerRow = meta_.width * bytesPerPixel;
  layout.rowsPerImage = meta_.height;

  WGPUExtent3D extent = {};
  extent.width = meta_.width;
  extent.height = meta_.height;
  extent.depthOrArrayLayers = meta_.depth;

  wgpuQueueWriteTexture(queue, &dst, srcPtr, srcSize, &layout, &extent);
  return tex;
}

}  // namespace somarender
