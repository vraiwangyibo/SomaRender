# 最小化 Demo 任务拆分

目标：加载体数据 + 三维体渲染（最简单版本），验证架构可行。

---

## 阶段 0：环境与骨架

| 序号 | 任务 | 说明 | 产出 |
|------|------|------|------|
| 0.1 | 搭建 C++ 工程与依赖 | CMake；集成 Dawn（子模块或预编译）、GLFW（窗口/事件）、Dear ImGui（可选，首版可无 UI） | 可编译、可打开窗口 |
| 0.2 | 初始化 WebGPU 设备与表面 | 创建 Dawn Device、Queue；GLFW 窗口 → WGPUSurface → 配置 SwapChain | 每帧清屏到单一颜色 |

---

## 阶段 1：体数据加载（最小化）

| 序号 | 任务 | 说明 | 产出 |
|------|------|------|------|
| 1.1 | 实现 Raw 体数据加载器 | 约定格式：二进制 Raw（如 8/16 bit 标量）+ 简单头或固定尺寸（如 256³）；读入到 `std::vector<uint8_t>` 或 `uint16_t` | 能从文件加载到内存的 3D 数组 |
| 1.2 | 上传为 3D 纹理 | 用 WebGPU API 创建 WGPUTexture（3D、R8 或 R16）、上传数据；保存尺寸等元数据供 Shader 使用 | GPU 上有一张 Volume 3D Texture |

*说明：首版不做 DICOM，用 Raw 快速打通管线；DICOM 可后续接在 Loader 前做解析再转成同一内存布局。*

---

## 阶段 2：体渲染管线（最简单版）

| 序号 | 任务 | 说明 | 产出 |
|------|------|------|------|
| 2.1 | 全屏四边形 + 相机 | 画一个全屏三角形/四边形；在 CPU 或 Shader 中算好 view、projection 矩阵（简单透视 + 看向体中心）；uniform 传入 | 能对「假想体」做射线 |
| 2.2 | Ray–Box 求 entry/exit | 在 Fragment Shader 里：从像素发射线，与单位立方体 [0,1]³ 求交，得到 rayStart、rayEnd（或 tmin/tmax） | 每条射线有步进起点与终点 |
| 2.3 | Ray Marching 循环 | 在 [rayStart, rayEnd] 间固定步长步进；每步采样 3D 纹理，用标量值查 TF（见下），前向合成颜色与不透明度；早停（alpha 接近 1） | 得到体绘制结果 |
| 2.4 | 最简 TF | 首版：在 Shader 里写死线性映射（如 value → opacity、灰度或单色），不单独做 1D 纹理；或做一张 256 的 1D 纹理用 R8/R8G8B8A8 | 能看到体数据的大致形状 |

---

## 阶段 3：串起来与可交互

| 序号 | 任务 | 说明 | 产出 |
|------|------|------|------|
| 3.1 | 绑定体数据与渲染 | 确保 Loader 在渲染前执行；Volume 纹理、uniform（分辨率、步长等）传入 Ray Marching Pass；一帧内完成：清屏 → 画全屏 quad → 体绘制 | 打开程序即可看到体渲染结果 |
| 3.2 | 简单相机交互（可选） | 鼠标拖拽旋转、滚轮缩放（Orbit 相机）；或固定视角先不交互 | 能转动/缩放查看体数据 |

---

## 依赖关系简图

```
0.1 → 0.2 → 2.1 → 2.2 → 2.3 → 2.4
  ↓      ↓
1.1 → 1.2 ──────────────→ 3.1 → 3.2
```

- 阶段 0 必须先完成。
- 阶段 1 可与 2.1～2.4 并行开发，最后在 3.1 汇合。
- 2.1～2.4 建议按顺序做（先有射线，再步进，再 TF）。

---

## 建议的代码/目录划分（最小化）

```
SomaRender/
  CMakeLists.txt
  src/
    main.cpp              # 入口、窗口、主循环
    gpu_context.cpp/h     # WebGPU 设备、SwapChain、表面
    volume_loader.cpp/h   # Raw 加载 + 上传 3D 纹理
    camera.cpp/h          # 视图/投影矩阵（可选，首版可写死在 main）
  shaders/
    volume_raster.vert    # 全屏 quad
    volume_raycast.frag   # Ray–Box + Ray Marching + TF 采样
  assets/
    (示例 small.raw 等)
```

---

## 验收标准（最小化 Demo）

1. 启动后打开一个窗口，内部显示**体渲染结果**（任意一个 Raw 体数据）。
2. 能看到体的大致形状（不要求画质、不要求 TF 可调）。
3. （可选）鼠标可旋转/缩放视角。

达到以上即算最小化 demo 完成，再在此基础上加 DICOM、可调 TF、LOD 等。
