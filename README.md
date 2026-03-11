# SomaRender

以 WebGPU (Dawn) 为核心的 C++ 三维医疗体数据渲染最小化 Demo。

## 依赖

- **CMake** 3.18+
- **C++20** 编译器
- **GLFW3**
- **Dawn**（WebGPU 原生实现）：需已构建并安装或设置 `DAWN_DIR`

## 构建

```bash
mkdir build && cd build
cmake .. -DDAWN_DIR=/path/to/dawn
cmake --build .
```

运行（需在可找到 `shaders/` 的目录下，或从 build 目录）：

```bash
./SomaRender [volume.raw [width height depth]]
```

默认加载 `assets/volume.raw`，尺寸 256×256×256。鼠标拖拽旋转，上下键缩放。

## 目录

- `src/` 应用与 RHI：main、gpu_context、volume_loader、camera
- `shaders/` WGSL：全屏三角形 + 体绘制 Ray Marching
- `assets/` 体数据（见 assets/README.md）