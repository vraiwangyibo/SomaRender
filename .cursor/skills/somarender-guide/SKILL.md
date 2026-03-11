---
name: somarender-guide
description: Encodes architecture, implementation decisions, and caveats for SomaRender (WebGPU C++ medical volume renderer). Use when modifying or extending SomaRender, adding volume loading, ray marching, Dawn/GLFW integration, or when the user refers to plan, demo tasks, or design decisions.
---

# SomaRender 项目要点

在修改或扩展 SomaRender 时遵循以下约定与已知坑，避免重复踩坑。

## 架构（与 plan.md 一致）

- **三层**：应用逻辑层（UI、DICOM 调度、TF 编辑）→ 图形抽象层（webgpu.h，资源/管线抽象）→ 后端驱动（Dawn Native）。
- **跨平台语义**：Native = C++ + Dawn；浏览器端需单独方案（WASM + JS 绑定 或 独立 Web 前端），不要和「Dawn (Native)」混为一谈。
- **DICOM**：首版用 Raw 打通管线；DICOM 解析（DCMTK/GDCM 或自研）接在 Loader 前，输出同一内存布局再上传。

## 体数据与加载

- **Volume Loader**：支持无头 Raw（`loadRaw(path, w, h, d, bits16)`）和 16 字节头 Raw（`loadRawWithHeader(path)`）；输出 GPU 3D 纹理 + `VolumeMetadata`（width/height/depth/is16Bit）。
- **上传格式**：为与 WGSL `texture_3d<f32>` 一致，16-bit 数据在上传时转为 R8Unorm（高位字节），单一路径 Shader 只处理 f32 采样。

## 体绘制管线

- **全屏**：全屏三角形（无顶点缓冲），Vertex 用 `vertex_index` 生成 NDC 坐标。
- **射线**：Fragment 中由 `invViewProj` + `cameraPos` 得到射线；与 [0,1]³ 做 Ray–Box 求 entry/exit，再固定步长 Ray Marching。
- **TF**：最简版可在 Shader 内写死（value → opacity/灰度）；可扩展为 1D/2D 纹理供采样。
- **窗宽窗位**：可合入 TF 或做前置线性映射。

## Dawn / WebGPU 与帧流程

- **Instance**：使用 `dawn::native::Instance`，`instance_ = dawnInstance.Get()`；部分 Dawn 构建可能为 `GetWGPUInstance()`，若编译报错可替换。
- **异步**：RequestAdapter / RequestDevice 为异步，需在循环中调用 `dawnInstance.ProcessEvents()` 直到 callback 置位。
- **Surface**：从 GLFW 创建 WGPUSurface 为平台相关：Windows 用 `SurfaceDescriptorFromWindowsHWND`，Linux 用 `SurfaceDescriptorFromXlibWindow`；macOS 需单独实现。
- **帧流程**：每帧先 `beginFrame()` 取得当前 backbuffer view，再 render，最后 `present()`；不要在 present 里才取 view。

## Uniform 与 WGSL 对齐

- 体绘制参数（invViewProj、cameraPos、resolution、volumeSize、stepSize）对应 WGSL `Params`，需满足 16 字节对齐（vec3 后补 pad）。
- C++ 侧 `UniformParams` 为 128 字节，含显式 padding；用 `static_assert(sizeof(UniformParams) == 128)` 保证与 Shader 一致。

## 任务顺序（与 demo-tasks.md 一致）

- 阶段 0（环境、SwapChain）→ 阶段 2.1–2.4（全屏、Ray–Box、Ray Marching、TF）与 阶段 1（Loader、上传）可并行，最后在 3.1 串联。
- 先有射线与步进，再加可调 TF、DICOM、LOD/空域跳过等。

## 扩展时注意

- 大体积：后续可加 LOD、空域跳过、步长/分辨率策略。
- 可调 TF：TF 表 → 1D/2D 纹理，ImGui 曲线/色条编辑后更新纹理。
