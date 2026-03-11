我希望实现一个以 WebGPU 为核心的 C++ 跨平台的三维医疗数据渲染系统，计划这样的架构

层次,核心职责,技术选型
应用逻辑层 (App Layer),UI 交互、DICOM 加载调度、传输函数（TF）编辑。,C++20 / Dear ImGui；DICOM 解析待定（DCMTK / GDCM 或自研最小解析）
图形抽象层 (RHI Layer),将资源（纹理、Buffer）和管线（Pipeline）抽象。,webgpu.h (标准头文件)
后端驱动层 (Backend),真正把指令发给 GPU。,Dawn (Native)；浏览器端若需支持可后续采用 WASM + JS 绑定或独立 Web 前端

主要包含以下核心模块：

A. 3D 数据加载器 (Volume Loader)
- 输入：DICOM 序列（或 Raw / NIfTI 等，可选）
- 输出：统一为 GPU 3D 纹理 + 元数据（spacing、origin 等）

B. 实时传输函数引擎 (Transfer Function Engine)
- 1D TF：标量强度 → 颜色 / 不透明度（可扩展 2D：强度 + 梯度模长）
- 实现：TF 表 → 1D/2D 纹理，供 Shader 采样；编辑端用 ImGui 曲线/色条

C. Ray Marching 渲染核 (The Shader)
- 依赖：相机与交互（视图/投影、旋转缩放）、Ray–Volume 相交（entry/exit 点或双 pass 纹理）
- 窗宽窗位（WW/WL）可合入 TF 或做前置线性映射

后续优化（大体积时）：LOD、空域跳过、步长与分辨率策略等。